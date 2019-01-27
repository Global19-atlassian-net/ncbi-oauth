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

#include "json-priv.hpp"

#include <assert.h>
#include <ctype.h>
#include <codecvt>
#include <locale>

namespace ncbi
{
    JSON :: Limits JSON :: default_limits;

    std :: string double_to_string ( long double val, unsigned int precision )
    {
        // TBD - come up with a more precise cutoff
        if ( precision > 40 )
            precision = 40;
        
        char buffer [ 1024 ];
        int len = std :: snprintf ( buffer, sizeof buffer, "%.*Lg", precision, val );
        if ( len < 0 || ( size_t ) len >= sizeof buffer )
            throw JSONException ( __func__, __LINE__, "failed to convert long double to string" );
        
        return std :: string ( buffer, len );
    }
    
    // skip whitespace
    // return the position of the first not whitespace character or npos
    static
    bool skip_whitespace ( const std :: string & text, size_t & pos )
    {
        size_t count = text . size ();

        while ( pos < count )
        {
            if ( ! isspace ( text [ pos ] ) )
                break;

            ++ pos;
        }

        if ( pos >= count )
        {
            pos = std::string::npos;
            return false;
        }

        return true;
    }

    static
    std :: string hex_to_utf8 ( const std :: string &text )
    {
        size_t index;

        try
        {
            unsigned int val = stoi ( text, &index, 16 );
            if ( index != 4 )
                throw JSONException ( __func__, __LINE__, "Invalid \\u escape sequence" ); // test hit

            std :: wstring_convert < std :: codecvt_utf8 < char32_t >, char32_t > conv;
            std :: string utf8 = conv . to_bytes ( val );

            return utf8;
        }
        catch ( ... )
        {
            throw JSONException ( __func__, __LINE__, "Invalid \\u escape sequence" ); // test hit
        }
    }

    static
    void test_wellformed_utf8 ( const std :: string & text )
    {
        const char * cp = text . data ();
        size_t i, count = text . size ();
        for ( i = 0; i < count; )
        {
            /*

              all bytes where the MSB is 0 are ASCII,
              i.e. single-byte characters. The value '\0'
              is not accepted.

              it is convenient to use signed bytes to examine
              the string and detect MSB as negative values.

             */
            while ( cp [ i ] > 0 )
            {
                if ( ++ i == count )
                    return;
            }

            /*

              an extended UTF-8 formatted multi-byte Unicode character
              has a start byte with the high 2..6 bits set, which indicates
              the overall length of the character.

              for UTF-8, the follow are legal start bytes:
                0b110xxxxx = 2 byte character
                0b1110xxxx = 3 byte character
                0b11110xxx = 4 byte character
                0b111110xx = 5 byte character
                0b1111110x = 6 byte character

              it is convenient in C to invert the bits as follows:
                0b110xxxxx => 0b001xxxxx
                0b1110xxxx => 0b0001xxxx
                0b11110xxx => 0b00001xxx
                0b111110xx => 0b000001xx
                0b1111110x => 0b0000001x
              since this allows use of a builtin function to count
              the leading 0's, which tells us immediately the length
              of the character.

            */


            // get complement of signed start byte
            unsigned int leading = ~ cp [ i ];

            // disallow 0, which comes from 0b11111111
            // since UTF-8 start bytes require at least one 0,
            // and the builtin function cannot operate on it.
            if ( leading == 0 )
                throw JSONException ( __func__, __LINE__, "malformed UTF-8" );

            // the bit calculations rely upon knowing the word size
            assert ( sizeof leading == 4 );

            // determine the character length by the number of leading zeros
            // only interested in the lower byte, so disregard upper 24 bits
            int char_len = ( int ) __builtin_clz ( leading ) - 24;

            // legal extended UTF-8 characters are 2..6 bytes long
            if ( char_len < 2 || char_len > 6 )
                throw JSONException ( __func__, __LINE__, "malformed UTF-8" );

            // the string must actually be large enough to contain these
            if ( i + char_len > count )
                throw JSONException ( __func__, __LINE__, "malformed UTF-8" );

            // the remaining bytes of the character all MUST begin
            // with the pattern 0b10xxxxxx. we can examine these
            // while building the Unicode value into UTF-32 format
            unsigned int utf32 = ~ leading & ( 0x7FU >> char_len );
            for ( int j = 1; j < char_len; ++ j )
            {
                unsigned int ch = ( ( const unsigned char * ) cp ) [ i + j ];
                if ( ( ch & 0xC0 ) != 0x80 )
                    throw JSONException ( __func__, __LINE__, "malformed UTF-8" );
                utf32 = ( utf32 << 6 ) | ( ch & 0x3F );
            }

            // TBD - examine code for validity in Unicode

            // account for multi-byte character
            i += char_len;
        }
    }
    
    static
    void test_depth ( const JSON :: Limits & lim, unsigned int & depth )
    {
        if ( ++ depth > lim . recursion_depth )
            throw JSONException ( __func__, __LINE__, "parsing recursion exceeds maximum depth" );
    }

    JSONValueRef JSON :: parse ( const std :: string & json )
    {
        return parse ( default_limits, json );
    }

    JSONValueRef JSON :: parse ( const Limits & lim, const std :: string & json )
    {
        if ( json . empty () )
            throw MalformedJSON ( __func__, __LINE__, "Empty JSON source" );

        if ( json . size () > ( size_t ) lim . json_string_size )
        {
            throw JSONLimitViolation ( __func__, __LINE__
                                       , "JSON source size ( %zu ) "
                                         "exceeds allowed size limit ( %u )"
                                       , json . size ()
                                       , lim . json_string_size
                );
        }

        size_t pos = 0;

        if ( ! skip_whitespace ( json, pos ) )
            throw MalformedJSON ( __func__, __LINE__, "Expected: '{' or '[' at offset %zu", pos );

        JSONValueRef val;

        switch ( json [ pos ] )
        {
        case '{':
            val = parseObject ( lim, json, pos, 0 );
            break;
        case '[':
            val = parseArray ( lim, json, pos, 0 );
            break;
        default:
            throw MalformedJSON ( __func__, __LINE__, "Expected: '{' or '[' at offset %zu", pos );
        }

        if ( pos < json . size () )
            throw MalformedJSON ( __func__, __LINE__, "Trailing byes in JSON text at offset %zu", pos );

        return val;
    }

    JSONObjectRef JSON :: parseObject ( const std :: string & json )
    {
        return parseObject ( default_limits, json );
    }

    JSONObjectRef JSON :: parseObject ( const Limits & lim, const std :: string & json )
    {
        if ( json . empty () )
            throw MalformedJSON ( __func__, __LINE__, "Empty JSON source" );

        if ( json . size () > ( size_t ) lim . json_string_size )
        {
            throw JSONLimitViolation ( __func__, __LINE__
                                       , "JSON source size ( %zu ) "
                                         "exceeds allowed size limit ( %u )"
                                       , json . size ()
                                       , lim . json_string_size
                );
        }

        size_t pos = 0;

        if ( ! skip_whitespace ( json, pos ) )
            throw MalformedJSON ( __func__, __LINE__, "Expected: '{' at offset %zu", pos );

        JSONObjectRef obj;

        switch ( json [ pos ] )
        {
        case '{':
            obj = parseObject ( lim, json, pos, 0 );
            break;
        default:
            throw JSONException ( __func__, __LINE__, "Expected: '{' at offset %zu", pos );
        }

        if ( pos < json . size () )
            throw MalformedJSON ( __func__, __LINE__, "Trailing byes in JSON text at offset %zu", pos );

        return obj;
    }

    JSONValueRef JSON :: makeNull ()
    {
        return JSONValueRef ( new JSONWrapper ( jvt_null ) );
    }

    JSONValueRef JSON :: makeBoolean ( bool val )
    {
        return JSONValueRef ( new JSONWrapper ( jvt_bool, new JSONBoolean ( val ) ) );
    }

    JSONValueRef JSON :: makeNumber ( const std :: string & val )
    {
        size_t pos = 0;
        return parseNumber ( default_limits, val, pos );
    }

    JSONValueRef JSON :: makeInteger ( long long int val )
    {
        return JSONValueRef ( new JSONWrapper ( jvt_int, new JSONInteger ( val ) ) );
    }

    JSONValueRef JSON :: makeDouble ( long double val, unsigned int precision )
    {
        return makeParsedNumber ( double_to_string ( val, precision ) );
    }

    JSONValueRef JSON :: makeString ( const std :: string & str )
    {
        if ( str . size () > default_limits . string_size )
            throw JSONException ( __func__, __LINE__, "string size exceeds allowed limit" );

        // examine all characters for legal and well-formed UTF-8
        test_wellformed_utf8 ( str );
        
        JSONValueRef val = makeParsedString ( str );
        if ( val == nullptr )
            throw JSONException ( __func__, __LINE__, "Failed to make JSONValue" );
        
        return val;
    }

    JSONArrayRef JSON :: makeArray ()
    {
        return JSONArrayRef ( new JSONArray () );
    }

    JSONObjectRef JSON :: makeObject ()
    {
        return JSONObjectRef ( new JSONObject () );
    }
    
    JSONValueRef JSON :: parse ( const Limits & lim, const std :: string & json,
        size_t & pos, unsigned int depth )
    {
        if ( skip_whitespace ( json, pos ) )
        {
            switch ( json [ pos ] )
            {
                case '{':
                    return parseObject ( lim, json, pos, depth );
                case '[':
                    return parseArray ( lim, json, pos, depth );
                case '"':
                    return parseString ( lim, json, pos );
                case 'f':
                case 't':
                    return parseBoolean ( json, pos );
                case '-':
                    return parseNumber ( lim, json, pos );
                case 'n':
                    return parseNull ( json, pos );
                default:
                    if ( isdigit ( json [ pos ] ) )
                        return parseNumber ( lim, json, pos );

                    // garbage
                    throw JSONException ( __func__, __LINE__, "Invalid JSON format" ); // test hit
            }
        }

        return JSONValueRef ( nullptr );
    }

    JSONValueRef JSON :: parseNull ( const std :: string & json, size_t & pos )
    {
        assert ( json [ pos ] == 'n' );

        if ( json . compare ( pos, sizeof "null" - 1, "null" ) == 0 )
            pos += sizeof "null" - 1;
        else
            throw JSONException ( __func__, __LINE__, "Expected keyword: 'null'") ; // test hit

        if ( pos < json . size () && isalnum ( json [ pos ] ) )
            throw JSONException ( __func__, __LINE__, "Expected keyword: 'null'" ); // test hit

        return makeNull ();
    }

    JSONValueRef JSON :: parseBoolean ( const std :: string & json, size_t & pos )
    {
        assert ( json [ pos ] == 'f' || json [ pos ] == 't' );

        bool tf;
        size_t start = pos;

        if ( json . compare ( start, sizeof "false" - 1, "false" ) == 0 )
        {
            tf = false;
            pos += sizeof "false" - 1;
        }
        else if ( json . compare ( start, sizeof "true" - 1, "true" ) == 0 )
        {
            tf = true;
            pos += sizeof "true" - 1;
        }
        else if ( json [ start ] == 'f' )
            throw JSONException ( __func__, __LINE__, "Expected keyword: 'false'" ); // test hit
        else
            throw JSONException ( __func__, __LINE__, "Expected keyword: 'true'" ); // test hit

        // if there were any extra characters following identification of a valid bool token
        if ( pos < json . size () && isalnum ( json [ pos ] ) )
        {
            if ( json [ start ] == 'f' )
                throw JSONException ( __func__, __LINE__, "Expected keyword: 'false'" ); // test hit
            else
                throw JSONException ( __func__, __LINE__, "Expected keyword: 'true'" ); // test hit
        }

        JSONValueRef val = makeBoolean ( tf );
        if ( val == nullptr )
            throw JSONException ( __func__, __LINE__, "Failed to make JSONValue" );

        return val;
    }

    JSONValueRef JSON :: parseNumber ( const Limits & lim, const std :: string & json, size_t & pos )
    {
        assert ( isdigit ( json [ pos ] ) || json [ pos ] == '-' );

        size_t start = pos;

        if ( json [ pos ] == '-' )
            ++ pos;

        if ( ! isdigit ( json [ pos ] ) )
            throw JSONException ( __func__, __LINE__, "Expected: digit" ); // test hit

        // check for 0
        if ( json [ pos ] == '0' )
            ++ pos;
        else
        {
            // just find the end of the number
            while ( isdigit ( json [ ++ pos ] ) )
                ;
        }

        bool is_float = false;
        switch ( json [ pos ] )
        {
            case '.':
            {
                // skip digits in search of float indicator
                while ( isdigit ( json [ ++ pos ] ) )
                    is_float = true;

                // must have at least one digit
                if ( ! is_float )
                    break; // we have an integer

                // if a character other than was [eE] found, break
                if ( toupper ( json [ pos ] ) != 'E' )
                    break;

                // no break - we have an [eE], fall through
            }
            case 'E':
            case 'e':
            {
                switch ( json [ ++ pos ] )
                {
                    case '+':
                    case '-':
                        ++ pos;
                        break;
                }

                while ( isdigit ( json [ pos ] ) )
                {
                    is_float = true;
                    ++ pos;
                }

                break;
            }
        }

        // check the number of total characters
        if ( pos - start > lim . numeral_length )
            throw JSONException ( __func__, __LINE__, "numeral length exceeds allowed limit" );
        
        // "pos" could potentially be a little beyond the end of
        // a legitimate number - let the conversion routines tell us
        std :: string num_str = json . substr ( start, pos - start );

        size_t num_len = 0;
        if ( ! is_float )
        {
            try
            {
                long long int num = std :: stoll ( num_str, &num_len );
                pos = start + num_len;

                return makeInteger ( num );
            }
            catch ( std :: out_of_range &e )
            {
                // fall out
            }
        }

        // must be floating point
        std :: stold ( num_str, &num_len );
        pos = start + num_len;

        if ( num_len > lim . numeral_length )
            throw JSONException ( __func__, __LINE__, "numeral size exceeds allowed limit" );
        
        JSONValueRef val = makeParsedNumber ( num_str . substr ( 0, num_len ) );
        if ( val == nullptr )
            throw JSONException ( __func__, __LINE__, "Failed to make JSONValue" );

        return val;
    }

    JSONValueRef JSON :: parseString ( const Limits & lim, const std :: string & json, size_t & pos )
    {
        assert ( json [ pos ] == '"' );

        std :: string str;

        // Find ending '"' or control characters
        size_t esc = json . find_first_of ( "\\\"", ++ pos );
        if ( esc == std :: string :: npos )
            throw JSONException ( __func__, __LINE__, "Invalid ending of string format" ); // test hit

        while ( 1 )
        {
            // add everything before the escape in
            // to the new string
            if ( str . size () + ( esc - pos ) > lim . string_size )
                throw JSONException ( __func__, __LINE__, "string size exceeds allowed limit" );

            str += json . substr ( pos, esc - pos );
            pos = esc;

            // found end of string
            if ( json [ pos ] != '\\' )
                break;

            // found '\'
            switch ( json [ ++ pos ] )
            {
                case '"':
                    str += '"';
                    break;
                case '\\':
                    str += '\\';
                    break;
                case '/':
                    str += '/';
                    break;
                case 'b':
                    str += '\b';
                    break;
                case 'f':
                    str += '\f';
                    break;
                case 'n':
                    str += '\n';
                    break;
                case 'r':
                    str += '\r';
                    break;
                case 't':
                    str += '\t';
                    break;
                case 'u':
                {
                    // start at the element after 'u'
#pragma warning "still need to deal with this properly"
                    std :: string unicode = json . substr ( pos + 1, 4 );
                    std :: string utf8 = hex_to_utf8 ( unicode );

                    str += utf8;
                    pos += 4;

                    break;
                }

                default:
                    throw JSONException ( __func__, __LINE__, "Invalid escape character" ); // test hit
            }

            // skip escaped character
            ++ pos;

            // Find ending '"' or control characters
            esc = json . find_first_of ( "\\\"", pos );
            if ( esc == std :: string :: npos )
                throw JSONException ( __func__, __LINE__, "Invalid end of string format" ); // test hit
        }

        assert ( esc == pos );
        assert ( json [ pos ] == '"' );

        // set pos to point to next token
        ++ pos;

        if ( str . size () > lim . string_size )
            throw JSONException ( __func__, __LINE__, "string size exceeds allowed limit" );

        // examine all characters for legal and well-formed UTF-8
        test_wellformed_utf8 ( str );
        
        JSONValueRef val = makeParsedString ( str );
        if ( val == nullptr )
            throw JSONException ( __func__, __LINE__, "Failed to make JSONValue" );

        return val;
    }

    JSONArrayRef JSON :: parseArray ( const Limits & lim, const std :: string & json,
        size_t & pos, unsigned int depth )
    {
        assert ( json [ pos ] == '[' );

        JSONArrayRef array ( new JSONArray () );
        while ( 1 )
        {
            // skip over '[' and any whitespace
            // json [ 0 ] is '[' or ','
            if ( ! skip_whitespace ( json, ++ pos ) )
                throw JSONException ( __func__, __LINE__, "Expected: ']'" ); // test hit

            if ( json [ pos ] == ']' )
                break;

            // use scope to invalidate value
            {
                JSONValueRef value = parse ( lim, json, pos, depth );
                if ( value == nullptr )
                    throw JSONException ( __func__, __LINE__, "Failed to create JSON object" );

                array -> appendValue ( value );

                if ( array -> count () > default_limits . array_elem_count )
                    throw JSONException ( __func__, __LINE__, "Array element count exceeds limit" );
            }

            // find and skip over ',' and skip any whitespace
            // exit loop if no ',' found
            if ( ! skip_whitespace ( json, pos ) || json [ pos ] != ',' )
                break;
        }

        // must end on ']'
        if ( pos == std :: string :: npos || json [ pos ] != ']' )
            throw JSONException ( __func__, __LINE__, "Excpected: ']'" ); // Test hit

        // skip over ']'
        ++ pos;

        // JSONArray must be valid
        assert ( array != nullptr );
        return array;
    }
        
    JSONObjectRef JSON :: parseObject ( const Limits & lim, const std :: string & json,
        size_t & pos, unsigned int depth )
    {
        test_depth ( lim, depth );

        assert ( json [ pos ] == '{' );

        JSONObjectRef obj ( new JSONObject () );
        while ( 1 )
        {
            // skip over '{' and any whitespace
            // json [ pos ] is '{' or ',', start at json [ pos + 1 ]
            if ( ! skip_whitespace ( json, ++ pos ) )
                throw JSONException ( __func__, __LINE__, "Expected: '}'" ); // test hit

            if ( json [ pos ] == '}' )
                break;

            if ( json [ pos ] != '"' )
                throw JSONException ( __func__, __LINE__, "Expected: 'name' " );

            JSONValueRef name = parseString ( lim, json, pos );
            if ( name == nullptr )
                throw JSONException ( __func__, __LINE__, "Failed to create JSON object" );

            // skip to ':'
            if ( ! skip_whitespace ( json, pos ) || json [ pos ] != ':' )
                throw JSONException ( __func__, __LINE__, "Expected: ':'" ); // test hit
                
            // skip over ':'
            ++ pos;
                
            // get JSON value;
            {
                JSONValueRef value = parse ( lim, json, pos, depth );
                if ( value == nullptr )
                    throw JSONException ( __func__, __LINE__, "Failed to create JSON object" );
                            
                obj -> addValue ( name -> toString (), value );
            }
                
            if ( obj -> count () > default_limits . object_mbr_count )
                throw JSONException ( __func__, __LINE__, "Array element count exceeds limit" );

            // find and skip over ',' and skip any whitespace
            // exit loop if no ',' found
            if ( ! skip_whitespace ( json, pos ) || json [ pos ] != ',' )
                break;
        }

        // must end on '}'
        if ( pos == std :: string :: npos || json [ pos ] != '}' )
            throw JSONException ( __func__, __LINE__, "Expected: '}'" ); // test hit

        // skip over '}'
        ++ pos;

        // JSONObject must be valid
        assert ( obj != nullptr );
        return obj;
    }

    JSONValueRef JSON :: makeParsedNumber ( const std :: string & val )
    {
        return JSONValueRef ( new JSONWrapper ( jvt_num, new JSONNumber ( val ) ) );
    }

    JSONValueRef JSON :: makeParsedString ( const std :: string & val )
    {
        return JSONValueRef ( new JSONWrapper ( jvt_str, new JSONString ( val ) ) );
    }

    JSONValueRef JSON :: test_parse ( const std :: string & json, bool consume_all )
    {
        size_t pos = 0;
        if ( json . empty () )
            throw JSONException ( __func__, __LINE__, "Empty JSON source" );

        if ( json . size () > default_limits . json_string_size )
            throw JSONException ( __func__, __LINE__, "JSON source exceeds allowed size limit" );

        JSONValueRef val = parse ( default_limits, json, pos, 0 );

        if ( consume_all && pos < json . size () )
            throw JSONException ( __func__, __LINE__, "Trailing byes in JSON text" ); // test hit

        return val;
    }

    JSON :: Limits :: Limits ()
        : json_string_size ( 4 * 1024 * 1024 )
        , recursion_depth ( 32 )
        , numeral_length ( 256 )
        , string_size ( 64 * 1024 )
        , array_elem_count ( 4 * 1024 )
        , object_mbr_count ( 256 )
    {
    }
}


