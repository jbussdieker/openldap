/* modify.c - ldap backend modify function */
/* $OpenLDAP$ */
/*
 * Copyright 1998-1999 The OpenLDAP Foundation, All Rights Reserved.
 * COPYING RESTRICTIONS APPLY, see COPYRIGHT file
 */
/* This is an altered version */
/*
 * Copyright 1999, Howard Chu, All rights reserved. <hyc@highlandsun.com>
 * 
 * Permission is granted to anyone to use this software for any purpose
 * on any computer system, and to alter it and redistribute it, subject
 * to the following restrictions:
 * 
 * 1. The author is not responsible for the consequences of use of this
 *    software, no matter how awful, even if they arise from flaws in it.
 * 
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Since few users ever read sources,
 *    credits should appear in the documentation.
 * 
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.  Since few users
 *    ever read sources, credits should appear in the documentation.
 * 
 * 4. This notice may not be removed or altered.
 *
 *
 *
 * Copyright 2000, Pierangelo Masarati, All rights reserved. <ando@sys-net.it>
 * 
 * This software is being modified by Pierangelo Masarati.
 * The previously reported conditions apply to the modified code as well.
 * Changes in the original code are highlighted where required.
 * Credits for the original code go to the author, Howard Chu.
 */

#include "portable.h"

#include <stdio.h>

#include <ac/string.h>
#include <ac/socket.h>

#include "slap.h"
#include "back-ldap.h"

int
ldap_back_modify(
    Backend	*be,
    Connection	*conn,
    Operation	*op,
    const char	*dn,
    const char	*ndn,
    Modifications	*modlist
)
{
	struct ldapinfo	*li = (struct ldapinfo *) be->be_private;
	struct ldapconn *lc;
	LDAPMod **modv;
	LDAPMod *mods;
	Modifications *ml;
	int i;
	char *mdn, *mapped;

	lc = ldap_back_getconn(li, conn, op);
	if ( !lc || !ldap_back_dobind( lc, op ) ) {
		return( -1 );
	}

	mdn = ldap_back_dn_massage( li, ch_strdup( dn ), 0 );
	if ( mdn == NULL ) {
		return( -1 );
	}

	for (i=0, ml=modlist; ml; i++,ml=ml->sml_next)
		;

	mods = (LDAPMod *)ch_malloc(i*sizeof(LDAPMod));
	if (mods == NULL)
		return( -1 );
	modv = (LDAPMod **)ch_malloc((i+1)*sizeof(LDAPMod *));
	if (modv == NULL) {
		free(mods);
		return( -1 );
	}

	for (i=0, ml=modlist; ml; ml=ml->sml_next) {
		mapped = ldap_back_map(&li->at_map, ml->sml_desc->ad_cname->bv_val, 0);
		if (mapped != NULL) {
			modv[i] = &mods[i];
			mods[i].mod_op = ml->sml_op | LDAP_MOD_BVALUES;
			mods[i].mod_type = mapped;
			mods[i].mod_bvalues = ml->sml_bvalues;
			i++;
		}
	}
	modv[i] = 0;

	ldap_modify_s( lc->ld, mdn, modv );
	free( mdn );
	free(mods);
	free(modv);
	return( ldap_back_op_result( lc, op ));
}
