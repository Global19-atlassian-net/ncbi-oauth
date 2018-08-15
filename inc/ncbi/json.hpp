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

#ifndef _hpp_ncbi_oauth_json_
#define _hpp_ncbi_oauth_json_

#include <map>
#include <string>
#include <vector>
#include <stdexcept>

namespace ncbi
{
    class JSONArray;
    class JSONObject;
    
    /* JSONException
     **********************************************************************************/
    class JSONException : public std :: logic_error
    {
    public:

        virtual const char * what () const
            throw ();
        
        explicit JSONException ( const char * function, unsigned int line, const char * message );
        virtual ~JSONException ()
            throw ();
        
    private:
        
        std :: string msg;
        const char * fl_msg;
    };

    /* JSONValue interface
     **********************************************************************************/
    class JSONValue
    {
    public:
        
        struct Limits
        {
            Limits ();                        // set default limits
            
            unsigned int json_string_size;    // total size of JSON string
            unsigned int recursion_depth;     // parser stack depth
            unsigned int numeral_length;      // maximum number of characters in number
            unsigned int string_size;         // maximum number of bytes in string
            unsigned int array_elem_count;    // maximum number of elements in array
            unsigned int object_mbr_count;    // maximum number of members in object
        };

        // make various value types
        static JSONValue * makeNull ();
        static JSONValue * makeBool ( bool val );
        static JSONValue * makeInteger ( long long int val );
        static JSONValue * makeDouble ( long double val, unsigned int precision );
        static JSONValue * makeNumber ( const std :: string & val );
        static JSONValue * makeString ( const std :: string & val );

        // query value type
        virtual bool isNull () const;
        virtual bool isBool () const;
        virtual bool isInteger () const;        // a number that is an integer
        virtual bool isNumber () const;         // is any type of number
        virtual bool isString () const;         // is specifically a string
        virtual bool isArray () const;
        virtual bool isObject () const;

        // set value - can change value type
        virtual JSONValue & setNull ();
        virtual JSONValue & setBool ( bool val );
        virtual JSONValue & setInteger ( long long int val );
        virtual JSONValue & setDouble ( long double val, unsigned int precision );
        virtual JSONValue & setNumber ( const std :: string & val );
        virtual JSONValue & setString ( const std :: string & val );

        // retrieve a value - will attempt to convert if possible
        // throws an exception if conversion is not supported
        virtual bool toBool () const;
        virtual long long toInteger () const;
        virtual std :: string toNumber () const;
        virtual std :: string toString () const;
        virtual std :: string toJSON () const = 0;

        // retrieve as structured value - will not convert
        // throws an exception if not of the correct container type
        virtual JSONArray & toArray ();
        virtual const JSONArray & toArray () const;
        virtual JSONObject & toObject ();
        virtual const JSONObject & toObject () const;

        // create a copy
        virtual JSONValue * clone () const;
        
        virtual ~JSONValue ();

    protected:
        
        static JSONValue * parse ( const Limits & lim, const std :: string & json, size_t & pos, unsigned int depth );
        static Limits default_limits;
        
        JSONValue ();
        
        // for testing
        static JSONValue * test_parse ( const std :: string & json, bool consume_all );
        friend class JSONFixture_WhiteBox;
    };
        
    /* JSONArray 
     * array of JSONValues
     **********************************************************************************/
    class JSONArray : public JSONValue
    {
    public:

        // make an empty array
        static JSONArray * make ();

        // JSONValue interface implementations
        virtual bool isArray () const
        { return true; }
        virtual std :: string toString () const;
        virtual std :: string toJSON () const;
        virtual JSONArray & toArray ()
        { return * this; }
        virtual const JSONArray & toArray () const
        { return * this; }
        virtual JSONValue * clone () const;

        // asks whether array is empty
        bool isEmpty () const;

        // return the number of elements
        unsigned long int count () const;

        // does an element exist
        bool exists ( long int idx ) const;

        // add a new element to end of array
        void appendValue ( JSONValue * elem );

        // set entry to a new value
        // will fill any undefined intermediate elements with null values
        // throws exception on negative index
        void setValue ( long int idx, JSONValue * elem );

        // get value at index
        // throws exception on negative index or when element is undefined
        JSONValue & getValue ( long int idx );
        const JSONValue & getValue ( long int idx ) const;

        // remove and return an entry if valid
        // returns nullptr if index was negative or element undefined
        // replaces valid internal entries with null element
        // deletes trailing null elements making them undefined
        JSONValue * removeValue ( long int idx );
        
        // lock the array against change
        void lock ();

        // C++ assignment
        JSONArray & operator = ( const JSONArray & array );
        JSONArray ( const JSONArray & a );
        
        virtual ~JSONArray ();
        
    private:

        static JSONArray * parse ( const Limits & lim, const std :: string & json, size_t & pos, unsigned int depth );
        
        // used to empty out the array before copy
        void clear ();

        JSONArray ();

        std :: vector < JSONValue * > array;
        bool locked;
        
        friend class JSONValue;
        
        // for testing
        static JSONArray * test_parse ( const std :: string & json );
        friend class JSONFixture_WhiteBox;
    };
    
    /* JSONObject
     * map of key <string> / value <JSONValue*> pairs
     **********************************************************************************/
    class JSONObject : public JSONValue
    {
    public:

        // make an empty object
        static JSONObject * make ();

        // make an object from JSON source
        static JSONObject * make ( const std :: string & json );
        static JSONObject * make ( const Limits & lim, const std :: string & json );

        // JSONValue interface implementations
        virtual bool isObject () const
        { return true; }
        virtual std :: string toString () const;
        virtual std :: string toJSON () const;
        virtual JSONObject & toObject ()
        { return * this; }
        virtual const JSONObject & toObject () const
        { return * this; }
        virtual JSONValue * clone () const;

        // asks whether object is empty
        bool isEmpty () const;

        // does a member exist
        bool exists ( const std :: string & name ) const;

        // return the number of members
        unsigned long int count () const;
        
        // return names/keys
        std :: vector < std :: string > getNames () const;
        
        // set entry to a new value
        // throws exception if entry exists and is final
        void setValue ( const std :: string & name, JSONValue * val );

        // set entry to a final value
        // throws exception if entry exists and is final
        void setFinalValue ( const std :: string & name, JSONValue * val );

        // get named value
        JSONValue & getValue ( const std :: string & name );
        const JSONValue & getValue ( const std :: string & name ) const;
        
        // remove and delete named value
        void removeValue ( const std :: string & name );
        
        // lock the object against change
        void lock ();

        // C++ assignment
        JSONObject & operator = ( const JSONObject & obj );
        JSONObject ( const JSONObject & obj );

        virtual ~JSONObject ();

        
    private:
        
        static JSONObject * parse ( const Limits & lim, const std :: string & json, size_t & pos, unsigned int depth );

        void clear ();
        
        JSONObject ();

        std :: map < std :: string, std :: pair < bool, JSONValue * > > members;
        bool locked;
        
        friend class JSONValue;
    };

}
#endif /* _hpp_ncbi_oauth_json_ */
