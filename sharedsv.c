/*    sharedsv.c
 *
 *    Copyright (c) 2001, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
* Contributed by Arthur Bergman arthur@contiller.se
*
* "Hand any two wizards a piece of rope and they would instinctively pull in
* opposite directions."
*                         --Sourcery
*
*/

#include "EXTERN.h"
#define PERL_IN_SHAREDSV_C
#include "perl.h"

PerlInterpreter* sharedsv_space;

#ifdef USE_ITHREADS

/*
  Shared SV

  Shared SV is a structure for keeping the backend storage
  of shared svs.

 */

/*
=for apidoc sharedsv_init

Saves a space for keeping SVs wider than an interpreter,
currently only stores a pointer to the first interpreter.

=cut
*/

void
Perl_sharedsv_init(pTHX)
{
    sharedsv_space = PERL_GET_CONTEXT;
}

/*
=for apidoc sharedsv_new

Allocates a new shared sv struct, you must yourself create the SV/AV/HV.
=cut
*/

shared_sv *
Perl_sharedsv_new(pTHX)
{
    shared_sv* ssv;
    New(2555,ssv,1,shared_sv);
    MUTEX_INIT(&ssv->mutex);
    COND_INIT(&ssv->cond);
    ssv->locks = 0;
    return ssv;
}


/*
=for apidoc sharedsv_find

Tries to find if a given SV has a shared backend, either by
looking at magic, or by checking if it is tied again threads::shared.

=cut
*/

shared_sv *
Perl_sharedsv_find(pTHX_ SV* sv)
{
    /* does all it can to find a shared_sv struct, returns NULL otherwise */
    shared_sv* ssv = NULL;
    return ssv;
}

/*
=for apidoc sharedsv_lock

Recursive locks on a sharedsv.
Locks are dynamicly scoped at the level of the first lock.
=cut
*/
void
Perl_sharedsv_lock(pTHX_ shared_sv* ssv)
{
    if(!ssv)
        return;
    if(ssv->owner && ssv->owner == my_perl) {
        ssv->locks++;
        return;
    }
    MUTEX_LOCK(&ssv->mutex);
    ssv->locks++;
    ssv->owner = my_perl;
    if(ssv->locks == 1)
        SAVEDESTRUCTOR_X(Perl_sharedsv_unlock_scope,ssv);
}

/*
=for apidoc sharedsv_unlock

Recursively unlocks a shared sv.

=cut
*/

void
Perl_sharedsv_unlock(pTHX_ shared_sv* ssv)
{
    if(ssv->owner != my_perl)
        return;

    if(--ssv->locks == 0) {
        ssv->owner = NULL;
        MUTEX_UNLOCK(&ssv->mutex);
    }
 }

void
Perl_sharedsv_unlock_scope(pTHX_ shared_sv* ssv)
{
    if(ssv->owner != my_perl)
        return;
    ssv->locks = 0;
    ssv->owner = NULL;
    MUTEX_UNLOCK(&ssv->mutex);
}

/*
=for apidoc sharedsv_thrcnt_inc

Increments the threadcount of a sharedsv.
=cut
*/
void
Perl_sharedsv_thrcnt_inc(pTHX_ shared_sv* ssv)
{
  SHAREDSvLOCK(ssv);
  SvREFCNT_inc(ssv->sv);
  SHAREDSvUNLOCK(ssv);
}

/*
=for apidoc sharedsv_thrcnt_dec

Decrements the threadcount of a shared sv. When a threads frontend is freed
this function should be called.

=cut
*/

void
Perl_sharedsv_thrcnt_dec(pTHX_ shared_sv* ssv)
{
    SV* sv;
    SHAREDSvLOCK(ssv);
    SHAREDSvEDIT(ssv);
    sv = SHAREDSvGET(ssv);
    if (SvREFCNT(sv) == 1) {
        switch (SvTYPE(sv)) {
        case SVt_RV:
            if (SvROK(sv))
            Perl_sharedsv_thrcnt_dec(aTHX_ (shared_sv *)SvIV(SvRV(sv)));
            break;
        case SVt_PVAV: {
            SV **src_ary  = AvARRAY((AV *)sv);
            SSize_t items = AvFILLp((AV *)sv) + 1;

            while (items-- > 0) {
            if(SvTYPE(*src_ary))
                Perl_sharedsv_thrcnt_dec(aTHX_ (shared_sv *)SvIV(*src_ary++));
            }
            break;
        }
        case SVt_PVHV: {
            HE *entry;
            (void)hv_iterinit((HV *)sv);
            while ((entry = hv_iternext((HV *)sv)))
                Perl_sharedsv_thrcnt_dec(
                    aTHX_ (shared_sv *)SvIV(hv_iterval((HV *)sv, entry))
                );
            break;
        }
        }
    }
    SvREFCNT_dec(sv);
    SHAREDSvRELEASE(ssv);
    SHAREDSvUNLOCK(ssv);
}

#endif
