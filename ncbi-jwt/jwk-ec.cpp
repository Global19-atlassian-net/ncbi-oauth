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

#include <ncbi/json.hpp>
#include <ncbi/jwt.hpp>
#include <ncbi/jws.hpp>
#include <ncbi/jwk.hpp>
#include "base64-priv.hpp"

#include <iostream>

namespace ncbi
{
    const EllipticCurvePrivate_JWKey * EllipticCurvePrivate_JWKey :: make ( const std :: string & curve,
        const std :: string & use, const std :: string & alg, const std :: string & kid )
    {
        JSONObject * props = JSONObject :: make ();
        try
        {
            props -> setValueOrDelete ( "kty", JSONValue :: makeString ( "EC" ) );
            props -> setValueOrDelete ( "kid", JSONValue :: makeString ( kid ) );
            props -> setValueOrDelete ( "alg", JSONValue :: makeString ( alg ) );
            props -> setValueOrDelete ( "use", JSONValue :: makeString ( use ) );

            // TBD - create key and get properties

            return make ( props );
        }
        catch ( ... )
        {
            props -> invalidate ();
            delete props;
            throw;
        }
    }

    bool EllipticCurvePrivate_JWKey :: isEllipticCurve () const
    {
        return true;
    }

    bool EllipticCurvePrivate_JWKey :: isPrivate () const
    {
        return true;
    }

    const EllipticCurvePrivate_JWKey * EllipticCurvePrivate_JWKey :: toEllipticCurvePrivate () const
    {
        return reinterpret_cast < const EllipticCurvePrivate_JWKey * > ( duplicate () );
    }

    const EllipticCurvePublic_JWKey * EllipticCurvePrivate_JWKey :: toEllipticCurvePublic () const
    {
        return EllipticCurvePublic_JWKey :: derive ( this );
    }

    // curve "crv"
    std :: string EllipticCurvePrivate_JWKey :: getCurve () const
    {
        return props -> getValue ( "crv" ) . toString ();
    }

    // X coordinate "x"
    std :: string EllipticCurvePrivate_JWKey :: getXCoordinate () const
    {
        return props -> getValue ( "x" ) . toString ();
    }

    // Y coordinate "y"
    std :: string EllipticCurvePrivate_JWKey :: getYCoordinate () const
    {
        return props -> getValue ( "y" ) . toString ();
    }

    // ECC private key "d"
    std :: string EllipticCurvePrivate_JWKey :: getECCPrivateKey () const
    {
        return props -> getValue ( "d" ) . toString ();
    }

    EllipticCurvePrivate_JWKey * EllipticCurvePrivate_JWKey :: make ( JSONObject * props )
    {
        return new EllipticCurvePrivate_JWKey ( props );
    }

    // "kty" = "EC"
    EllipticCurvePrivate_JWKey :: EllipticCurvePrivate_JWKey ( JSONObject * props )
        : JWK ( props )
    {
    }


    const EllipticCurvePublic_JWKey * EllipticCurvePublic_JWKey :: derive ( const EllipticCurvePrivate_JWKey * priv )
    {
        JSONObject * props = JSONObject :: make ();
        try
        {
            props -> setValueOrDelete ( "kty", JSONValue :: makeString ( priv -> getType () ) );
            props -> setValueOrDelete ( "kid", JSONValue :: makeString ( priv -> getID () ) );
            props -> setValueOrDelete ( "alg", JSONValue :: makeString ( priv -> getAlg () ) );
            props -> setValueOrDelete ( "crv", JSONValue :: makeString ( priv -> getCurve () ) );
            props -> setValueOrDelete ( "x", JSONValue :: makeString ( priv -> getXCoordinate () ) );
            props -> setValueOrDelete ( "y", JSONValue :: makeString ( priv -> getYCoordinate () ) );
            return make ( props );
        }
        catch ( ... )
        {
            props -> invalidate ();
            delete props;
            throw;
        }
    }

    bool EllipticCurvePublic_JWKey :: isEllipticCurve () const
    {
        return true;
    }

     const EllipticCurvePublic_JWKey * EllipticCurvePublic_JWKey :: toEllipticCurvePublic () const
    {
        return reinterpret_cast < const EllipticCurvePublic_JWKey * > ( duplicate () );
    }

    // curve "crv"
    std :: string EllipticCurvePublic_JWKey :: getCurve () const
    {
        return props -> getValue ( "crv" ) . toString ();
    }

    // X coordinate "x"
    std :: string EllipticCurvePublic_JWKey :: getXCoordinate () const
    {
        return props -> getValue ( "x" ) . toString ();
    }

    // Y coordinate "y"
    std :: string EllipticCurvePublic_JWKey :: getYCoordinate () const
    {
        return props -> getValue ( "y" ) . toString ();
    }

    EllipticCurvePublic_JWKey * EllipticCurvePublic_JWKey :: make ( JSONObject * props )
    {
        // TBD - check values
        return new EllipticCurvePublic_JWKey ( props );
    }

    // "kty" = "EC"
    EllipticCurvePublic_JWKey :: EllipticCurvePublic_JWKey ( JSONObject * props )
        : JWK ( props )
    {
    }

}
