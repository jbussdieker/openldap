/*
 * Copyright 2000, OpenLDAP Foundation, All Rights Reserved.
 * COPYING RESTRICTIONS APPLY, see COPYRIGHT file
 */

#ifndef LDAP_CONNECTION_H
#define LDAP_CONNECTION_H

#include "LDAPSearchResults.h"
#include "LDAPExtResult.h"
#include "LDAPAsynConnection.h" 

/** Main class for synchronous LDAP-Communication
 *
 * The class represent a LDAP-Connection to perform synchronous
 * LDAP-Operations. This provides methodes for the different
 * LDAP-Operations. All the methods for the LDAP-operations block until
 * all results for the operation are received or until an error occurs
 */
class LDAPConnection : private LDAPAsynConnection {

    public :
        /** 
         * Constant for the Search-Operation to indicate a Base-Level
         * Search
         */
        static const int SEARCH_BASE;

        /** 
         * Constant for the Search-Operation to indicate a One-Level
         * Search
         */
        static const int SEARCH_ONE;
        
        /** 
         * Constant for the Search-Operation to indicate a Subtree 
         * Search
         */
        static const int SEARCH_SUB;
        
        /** This Constructor initializes synchronous LDAP-Connection 
         * 
         * During execution of this constructor no network communication
         * is performed. Just some internal data structure are initialized
         * @param hostname Name (or IP-Adress) of the destination host
         * @param port Port the LDAP server is running on
         * @param cons Default constraints to use with operations over 
         *      this connection
         */
        LDAPConnection(const string& hostname="localhost", int port=389,
                LDAPConstraints* cons=0);
        
        /**
         * Destructor
         */
        ~LDAPConnection();
        
        /** 
         * Initzializes a synchronous connection to a server. 
         * 
         * There is actually no
         * communication to the server. Just the object is initialized
         * (e.g. this method is called within the 
         * LDAPConnection(char*,int,LDAPConstraints) constructor.)
         * @param hostname  The Name or IP-Address of the destination
         *             LDAP-Server
         * @param port      The Network Port the server is running on
         */
        void init(const string& hostname, int port);
        
        /** 
         * Performs a simple authentication with the server
         *
         * @throws LDAPReferralException if a referral is received
         * @throws LDAPException for any other error occuring during the
         *              operation
         * @param dn    The name of the entry to bind as
         * @param passwd    The cleartext password for the entry
         */
        void bind(const string& dn="", const string& passwd="",
                LDAPConstraints* cons=0);
        
        /**
         * Performs the UNBIND-operation on the destination server
         * 
         * @throws LDAPException in any case of an error
         */
        void unbind();
        
        /**
         * Performs a COMPARE-operation on an entery of the destination 
         * server.
         *
         * @throws LDAPReferralException if a referral is received
         * @throws LDAPException for any other error occuring during the
         *              operation
         * @param dn    Distinguished name of the entry for which the compare
         *              should be performed
         * @param attr  An Attribute (one (!) value) to use for the
         *      compare operation
         * @param cons  A set of constraints that should be used with this
         *              request
         * @returns The result of the compare operation. true if the
         *      attr-parameter matched an Attribute of the entry. false if it
         *      did not match
         */
        bool compare(const string& dn, const LDAPAttribute& attr,
                LDAPConstraints* cons=0);
       
        /**
         * Deletes an entry from the directory
         *
         * This method performs the DELETE operation on the server
         * @throws LDAPReferralException if a referral is received
         * @throws LDAPException for any other error occuring during the
         *              operation
         * @param dn    Distinguished name of the entry that should be deleted
         * @param cons  A set of constraints that should be used with this
         *              request
         */
        void del(const string& dn, const LDAPConstraints* cons=0);
        
        /**
         * Use this method to perform the ADD-operation
         *
         * @throws LDAPReferralException if a referral is received
         * @throws LDAPException for any other error occuring during the
         *              operation
         * @param le    the entry to add to the directory
         * @param cons  A set of constraints that should be used with this
         *              request
         */
        void add(const LDAPEntry* le, const LDAPConstraints* cons=0);
        
        /**
         * To modify the attributes of an entry, this method can be used
         *
         * @throws LDAPReferralException if a referral is received
         * @throws LDAPException for any other error occuring during the
         *              operation
         * @param dn    The DN of the entry which should be modified
         * @param mods  A set of modifications for that entry.
         * @param cons  A set of constraints that should be used with this
         *              request
         */
        void modify(const string& dn, const LDAPModList* mods, 
                const LDAPConstraints* cons=0); 

        /**
         * This method performs the ModDN-operation.
         *
         * It can be used to rename or move an entry by modifing its DN.
         *
         * @throws LDAPReferralException if a referral is received
         * @throws LDAPException for any other error occuring during the
         *              operation
         * @param dn    The DN that should be modified
         * @param newRDN    If the RDN of the entry should be modified the
         *                  new RDN can be put here.
         * @param delOldRDN If the old RDN should be removed from the
         *                  entry's attribute this parameter has to be
         *                  "true"
         * @param newParentDN   If the entry should be moved inside the
         *                      DIT, the DN of the new parent of the entry
         *                      can be given here.
         * @param cons  A set of constraints that should be used with this
         *              request
         */
        void rename(const string& dn, const string& newRDN, 
                bool delOldRDN=false, const string& newParentDN="",
                const LDAPConstraints* cons=0);
        
        /**
         * This method can be used for the sync. SEARCH-operation.
         *
         * @throws LDAPReferralException if a referral is received
         * @throws LDAPException for any other error occuring during the
         *              operation
         * @param base The distinguished name of the starting point for the
         *      search
         * @param scope The scope of the search. Possible values: <BR> 
         *      LDAPAsynConnection::SEARCH_BASE, <BR> 
         *      LDAPAsynConnection::SEARCH_ONE, <BR>
         *      LDAPAsynConnection::SEARCH_SUB
         * @param filter The string representation of a search filter to
         *      use with this operation
         * @param attrsOnly true if only the attributes names (no values) 
         *      should be returned
         * @param cons A set of constraints that should be used with this
         *      request
         * @returns A pointer to a LDAPSearchResults-object that can be
         *      used to read the results of the search.
         */
        LDAPSearchResults* search(const string& base, int scope=0, 
                const string& filter="objectClass=*", 
                const StringList& attrs=StringList(), bool attrsOnly=false,
                const LDAPConstraints* cons=0);
       
        /**
         * This method is for extended LDAP-Operations.
         *
         * @throws LDAPReferralException if a referral is received
         * @throws LDAPException for any other error occuring during the
         *              operation
         * @param oid The Object Identifier of the Extended Operation that
         *          should be performed.
         * @param strint If the Extended Operation needs some additional
         *          data it can be passed to the server by this parameter.
         * @param cons A set of constraints that should be used with this
         *      request
         * @returns The result of the Extended Operation as an
         *      pointer to a LDAPExtResult-object.
         */
        LDAPExtResult* extOperation(const string& oid, const string&
                value="", const LDAPConstraints *const = 0);
        
        const string& getHost() const;

        int getPort() const;
        
        void setConstraints(LDAPConstraints *cons);
        
        const LDAPConstraints* getConstraints() const ;
};

#endif //LDAP_CONNECTION_H
