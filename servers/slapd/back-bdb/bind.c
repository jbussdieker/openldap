/* bind.c - bdb backend bind routine */
/* $OpenLDAP$ */
/*
 * Copyright 1998-2003 The OpenLDAP Foundation, All Rights Reserved.
 * COPYING RESTRICTIONS APPLY, see COPYRIGHT file
 */

#include "portable.h"

#include <stdio.h>
#include <ac/krb.h>
#include <ac/string.h>
#include <ac/unistd.h>

#include "back-bdb.h"
#include "external.h"

int
bdb_bind( Operation *op, SlapReply *rs )
{
	struct bdb_info *bdb = (struct bdb_info *) op->o_bd->be_private;
	Entry		*e;
	Attribute	*a;
	Entry		*matched;
#ifdef LDAP_API_FEATURE_X_OPENLDAP_V2_KBIND
	char		krbname[MAX_K_NAME_SZ + 1];
	AttributeDescription *krbattr = slap_schema.si_ad_krbName;
	struct berval	krbval;
	AUTH_DAT	ad;
#endif

	AttributeDescription *password = slap_schema.si_ad_userPassword;

	u_int32_t	locker;
	DB_LOCK		lock;

#ifdef NEW_LOGGING
	LDAP_LOG ( OPERATION, ARGS, "==> bdb_bind: dn: %s\n", op->o_req_dn.bv_val, 0, 0 );
#else
	Debug( LDAP_DEBUG_ARGS, "==> bdb_bind: dn: %s\n", op->o_req_dn.bv_val, 0, 0);
#endif

	/* allow noauth binds */
	if ( op->oq_bind.rb_method == LDAP_AUTH_SIMPLE && be_isroot_pw( op )) {
		ber_dupbv( &op->oq_bind.rb_edn, be_root_dn( op->o_bd ) );
		/* front end will send result */
		return LDAP_SUCCESS;
	}

	rs->sr_err = LOCK_ID(bdb->bi_dbenv, &locker);
	switch(rs->sr_err) {
	case 0:
		break;
	default:
		rs->sr_text = "internal error";
		send_ldap_result( op, rs );
		return rs->sr_err;
	}

dn2entry_retry:
	/* get entry with reader lock */
	rs->sr_err = bdb_dn2entry_r( op->o_bd, NULL, &op->o_req_ndn, &e, &matched, 0, locker, &lock );

	switch(rs->sr_err) {
	case DB_NOTFOUND:
	case 0:
		break;
	case LDAP_BUSY:
		send_ldap_error( op, rs, LDAP_BUSY, "ldap_server_busy" );
		LOCK_ID_FREE(bdb->bi_dbenv, locker);
		return LDAP_BUSY;
	case DB_LOCK_DEADLOCK:
	case DB_LOCK_NOTGRANTED:
		goto dn2entry_retry;
	default:
		send_ldap_error( op, rs, LDAP_OTHER, "internal error" );
		LOCK_ID_FREE(bdb->bi_dbenv, locker);
		return rs->sr_err;
	}

	if ( e == NULL ) {
		if( matched != NULL ) {
			rs->sr_ref = is_entry_referral( matched )
				? get_entry_referrals( op, matched )
				: NULL;
			if (rs->sr_ref)
				rs->sr_matched = ch_strdup( matched->e_name.bv_val );

			bdb_cache_return_entry_r( bdb->bi_dbenv, &bdb->bi_cache, matched, &lock );
			matched = NULL;

		} else {
			rs->sr_ref = referral_rewrite( default_referral,
				NULL, &op->o_req_dn, LDAP_SCOPE_DEFAULT );
		}

		if ( rs->sr_ref != NULL ) {
			rs->sr_err = LDAP_REFERRAL;
			send_ldap_result( op, rs );
			free( (char *)rs->sr_matched );
			ber_bvarray_free( rs->sr_ref );
			rs->sr_ref = NULL;
			rs->sr_matched = NULL;
		} else {
			rs->sr_err = LDAP_INVALID_CREDENTIALS;
			send_ldap_result( op, rs );
		}

		LOCK_ID_FREE(bdb->bi_dbenv, locker);

		return rs->sr_err;
	}

	ber_dupbv( &op->oq_bind.rb_edn, &e->e_name );

	/* check for deleted */
#ifdef BDB_SUBENTRIES
	if ( is_entry_subentry( e ) ) {
		/* entry is an subentry, don't allow bind */
#ifdef NEW_LOGGING
		LDAP_LOG ( OPERATION, DETAIL1, 
			"bdb_bind: entry is subentry\n", 0, 0, 0 );
#else
		Debug( LDAP_DEBUG_TRACE, "entry is subentry\n", 0,
			0, 0 );
#endif

		rs->sr_err = LDAP_INVALID_CREDENTIALS;
		send_ldap_result( op );

		goto done;
	}
#endif

#ifdef BDB_ALIASES
	if ( is_entry_alias( e ) ) {
		/* entry is an alias, don't allow bind */
#ifdef NEW_LOGGING
		LDAP_LOG ( OPERATION, DETAIL1, "bdb_bind: entry is alias\n", 0, 0, 0 );
#else
		Debug( LDAP_DEBUG_TRACE, "entry is alias\n", 0,
			0, 0 );
#endif

		send_ldap_error( op, rs, LDAP_ALIAS_PROBLEM, "entry is alias");

		goto done;
	}
#endif

	if ( is_entry_referral( e ) ) {
		/* entry is a referral, don't allow bind */
		rs->sr_ref = get_entry_referrals( op, e );

#ifdef NEW_LOGGING
		LDAP_LOG ( OPERATION, DETAIL1, 
			"bdb_bind: entry is referral\n", 0, 0, 0 );
#else
		Debug( LDAP_DEBUG_TRACE, "entry is referral\n", 0,
			0, 0 );
#endif

		if( rs->sr_ref != NULL ) {
			rs->sr_err = LDAP_REFERRAL;
			rs->sr_matched = e->e_name.bv_val;
			send_ldap_result( op, rs );
			ber_bvarray_free( rs->sr_ref );
			rs->sr_ref = NULL;
			rs->sr_matched = NULL;
		} else {
			rs->sr_err = LDAP_INVALID_CREDENTIALS;
			send_ldap_result( op, rs );
		}

		goto done;
	}

	switch ( op->oq_bind.rb_method ) {
	case LDAP_AUTH_SIMPLE:
		rs->sr_err = access_allowed( op, e,
			password, NULL, ACL_AUTH, NULL );
		if ( ! rs->sr_err ) {
			rs->sr_err = LDAP_INSUFFICIENT_ACCESS;
			send_ldap_result( op, rs );
			goto done;
		}

		if ( (a = attr_find( e->e_attrs, password )) == NULL ) {
			rs->sr_err = LDAP_INAPPROPRIATE_AUTH;
			send_ldap_result( op, rs );
			goto done;
		}

		if ( slap_passwd_check( op->o_conn, a, &op->oq_bind.rb_cred ) != 0 ) {
			rs->sr_err = LDAP_INVALID_CREDENTIALS;
			send_ldap_result( op, rs );
			goto done;
		}

		rs->sr_err = 0;
		break;

#ifdef LDAP_API_FEATURE_X_OPENLDAP_V2_KBIND
	case LDAP_AUTH_KRBV41:
		if ( krbv4_ldap_auth( op->o_bd, &op->oq_bind.rb_cred, &ad ) != LDAP_SUCCESS ) {
			rs->sr_err = LDAP_INVALID_CREDENTIALS,
			send_ldap_result( op );
			goto done;
		}

		rs->sr_err = access_allowed( op, e,
			krbattr, NULL, ACL_AUTH, NULL );
		if ( ! rs->sr_err ) {
			rs->sr_err = LDAP_INSUFFICIENT_ACCESS,
			send_ldap_result( op );
			goto done;
		}

		krbval.bv_len = sprintf( krbname, "%s%s%s@%s", ad.pname, *ad.pinst ? "."
			: "", ad.pinst, ad.prealm );

		if ( (a = attr_find( e->e_attrs, krbattr )) == NULL ) {
			/*
			 * no krbname values present: check against DN
			 */
			if ( strcasecmp( op->o_req_dn.bv_val, krbname ) == 0 ) {
				rs->sr_err = 0;
				break;
			}
			rs->sr_err = LDAP_INAPPROPRIATE_AUTH,
			send_ldap_result( op );
			goto done;

		} else {	/* look for krbname match */
			krbval.bv_val = krbname;

			if ( value_find( a->a_desc, a->a_vals, &krbval ) != 0 ) {
				rs->sr_err = LDAP_INVALID_CREDENTIALS;
				send_ldap_result( op );
				goto done;
			}
		}
		rs->sr_err = 0;
		break;

	case LDAP_AUTH_KRBV42:
		send_ldap_error( op, rs, LDAP_UNWILLING_TO_PERFORM,
			"Kerberos bind step 2 not supported" );
		goto done;
#endif

	default:
		send_ldap_error( op, rs, LDAP_STRONG_AUTH_NOT_SUPPORTED,
			"authentication method not supported" );
		goto done;
	}

done:
	/* free entry and reader lock */
	if( e != NULL ) {
		bdb_cache_return_entry_r( bdb->bi_dbenv, &bdb->bi_cache, e, &lock );
	}

	LOCK_ID_FREE(bdb->bi_dbenv, locker);

	/* front end will send result on success (rs->sr_err==0) */
	return rs->sr_err;
}
