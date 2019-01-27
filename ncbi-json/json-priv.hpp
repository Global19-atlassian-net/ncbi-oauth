/*==============================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 */

#ifndef _hpp_ncbi_json_priv_
#define _hpp_ncbi_json_priv_

// an attempt at getting memset_s() declared
#define __STDC_WANT_LIB_EXT1__ 1
#include <string.h>

#ifndef _hpp_ncbi_json_
#include <ncbi/json.hpp>
#endif

// here at least temporarily
// to work around the problems with memset() and memset_s()
#ifndef _hpp_ncbi_json_memset_priv_
#include "memset-priv.hpp"
#endif

#include <cstring>

namespace ncbi
{
    std :: string double_to_string ( long double val, unsigned int precision );
    std :: string string_to_json ( const std :: string & string );

    enum JSONValType
    {
        jvt_null,
        jvt_bool,
        jvt_int,
        jvt_num,
        jvt_str
    };
    
    struct JSONPrimitive
    {
        virtual std :: string toString () const = 0;
        virtual std :: string toJSON () const;
        virtual JSONPrimitive * clone () const = 0;
        virtual void invalidate () = 0;
        virtual ~ JSONPrimitive () {}
    };
    
    struct JSONBoolean : JSONPrimitive
    {
        //static JSONValue * parse ( const std::string &json, size_t & pos );
        
        bool toBoolean () const
        { return value; }

        virtual std :: string toString () const;

        virtual JSONPrimitive * clone () const
        { return new JSONBoolean ( value ); }

        virtual void invalidate ()
        { value = false; }
        
        JSONBoolean ( bool val )
            : value ( val )
        {
        }

        bool value;
    };

    struct JSONInteger : JSONPrimitive
    {
        long long int toInteger () const
        { return value; }

        virtual std :: string toString () const;

        virtual JSONPrimitive * clone () const
        { return new JSONInteger ( value ); }

        virtual void invalidate ()
        { value = 0; }
        
        JSONInteger ( long long int val )
            : value ( val )
        {
        }

        long long int value;
    };

    struct JSONNumber : JSONPrimitive
    {
        //static JSONValue * parse ( const JSON :: Limits & lim, const std::string &json, size_t & pos );
        
        virtual std :: string toString () const
        { return value; }

        virtual JSONPrimitive * clone () const
        { return new JSONNumber ( value ); }

        virtual void invalidate ()
        {
            size_t vsize = value . size ();
            memset_while_respecting_language_semantics
                ( ( void * ) value . data (), vsize, ' ', vsize, value . c_str () );
        }
        
        JSONNumber ( const std :: string & val )
            : value ( val )
        {
        }

        ~ JSONNumber ()
        {
        }

        std :: string value;
    };

    struct JSONString : JSONPrimitive
    {
        //static JSONValue * parse ( const JSON :: Limits & lim, const std::string &json, size_t & pos );
        
        virtual std :: string toString () const
        { return value; }

        virtual std :: string toJSON () const;

        virtual JSONPrimitive * clone () const
        { return new JSONString ( value ); }

        virtual void invalidate ()
        {
            size_t vsize = value . size ();
            memset_while_respecting_language_semantics
                ( ( void * ) value . data (), vsize, ' ', vsize, value . c_str () );
        }
        
        JSONString ( const std :: string & val )
            : value ( val )
        {
        }

        ~ JSONString ()
        {
        }

        std :: string value;
    };
    
    struct JSONWrapper : JSONValue
    {
        virtual bool isNull () const override;
        virtual bool isBoolean () const override;
        virtual bool isNumber () const override;         // is any type of number
        virtual bool isInteger () const override;        // a number that is an integer
        virtual bool isString () const override;         // is specifically a string

        virtual JSONValue & setNull () override;
        virtual JSONValue & setBoolean ( bool val ) override;
        virtual JSONValue & setNumber ( const std :: string & val ) override;
        virtual JSONValue & setInteger ( long long int val ) override;
        virtual JSONValue & setDouble ( long double val, unsigned int precision ) override;
        virtual JSONValue & setString ( const std :: string & val ) override;
        
        virtual bool toBoolean () const override;
        virtual std :: string toNumber () const override;
        virtual long long toInteger () const override;
        virtual std :: string toString () const override;
        virtual std :: string toJSON () const override;

        virtual JSONValueRef clone () const override;

        virtual void invalidate () override;

        // C++ assignment
        JSONWrapper & operator = ( const JSONWrapper & val );
        JSONWrapper ( const JSONWrapper & val );

        JSONWrapper ( JSONValType type );
        JSONWrapper ( JSONValType type, JSONPrimitive * value );
        virtual ~ JSONWrapper ();

        JSONPrimitive * value; // wrapped value from library
        JSONValType type;
    };
}

#endif // _hpp_ncbi_json_priv_

