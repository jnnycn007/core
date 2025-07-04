/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <connectivity/ConnectionWrapper.hxx>
#include <com/sun/star/beans/PropertyValue.hpp>
#include <com/sun/star/sdbc/XConnection.hpp>
#include <comphelper/uno3.hxx>
#include <comphelper/sequence.hxx>
#include <comphelper/servicehelper.hxx>
#include <comphelper/hash.hxx>
#include <cppuhelper/supportsservice.hxx>
#include <com/sun/star/reflection/ProxyFactory.hpp>
#include <algorithm>

using namespace connectivity;

using namespace com::sun::star::uno;
using namespace com::sun::star::lang;
using namespace com::sun::star::beans;
using namespace com::sun::star::sdbc;
using namespace ::com::sun::star::reflection;

OConnectionWrapper::OConnectionWrapper()
    : OConnection_BASE(m_aMutex)
{

}

void OConnectionWrapper::setDelegation(Reference< XAggregation >& _rxProxyConnection)
{
    // So far only called from constructors
    OSL_ENSURE(_rxProxyConnection.is(),"OConnectionWrapper: Connection must be valid!");
    osl_atomic_increment(&m_refCount);
    if (_rxProxyConnection.is())
    {
        // transfer the (one and only) real ref to the aggregate to our member
        m_xProxyConnection = _rxProxyConnection;
        _rxProxyConnection = nullptr;
        ::comphelper::query_aggregation(m_xProxyConnection,m_xConnection);
        m_xTypeProvider.set(m_xConnection,UNO_QUERY);
        m_xUnoTunnel.set(m_xConnection,UNO_QUERY);
        m_xServiceInfo.set(m_xConnection,UNO_QUERY);

        // set ourself as delegator
        Reference<XInterface> xIf = static_cast< XUnoTunnel* >( this );
        m_xProxyConnection->setDelegator( xIf );

    }
    osl_atomic_decrement(&m_refCount);
}

void OConnectionWrapper::setDelegation(const Reference< XConnection >& _xConnection
                                       ,const Reference< XComponentContext>& _rxContext)
{
    // So far only called from constructors
    OSL_ENSURE(_xConnection.is(),"OConnectionWrapper: Connection must be valid!");
    osl_atomic_increment(&m_refCount);

    m_xConnection = _xConnection;
    m_xTypeProvider.set(m_xConnection,UNO_QUERY);
    m_xUnoTunnel.set(m_xConnection,UNO_QUERY);
    m_xServiceInfo.set(m_xConnection,UNO_QUERY);

    Reference< XProxyFactory >  xProxyFactory = ProxyFactory::create( _rxContext );
    Reference< XAggregation > xConProxy = xProxyFactory->createProxy(_xConnection);
    if (xConProxy.is())
    {
        // transfer the (one and only) real ref to the aggregate to our member
        m_xProxyConnection = std::move(xConProxy);

        // set ourself as delegator
        Reference<XInterface> xIf = static_cast< XUnoTunnel* >( this );
        m_xProxyConnection->setDelegator( xIf );

    }
    osl_atomic_decrement(&m_refCount);
}

void OConnectionWrapper::disposing()
{
    osl::MutexGuard aGuard(m_aMutex);
    OConnection_BASE::disposing();
    m_xConnection.clear();
}

OConnectionWrapper::~OConnectionWrapper()
{
    if (m_xProxyConnection.is())
        m_xProxyConnection->setDelegator(nullptr);
}

// XServiceInfo

OUString SAL_CALL OConnectionWrapper::getImplementationName(  )
{
    return u"com.sun.star.sdbc.drivers.OConnectionWrapper"_ustr;
}


css::uno::Sequence< OUString > SAL_CALL OConnectionWrapper::getSupportedServiceNames(  )
{
    osl::MutexGuard aGuard(m_aMutex);
    // first collect the services which are supported by our aggregate
    Sequence< OUString > aSupported;
    if ( m_xServiceInfo.is() )
        aSupported = m_xServiceInfo->getSupportedServiceNames();

    // append our own service, if necessary
    OUString sConnectionService( u"com.sun.star.sdbc.Connection"_ustr );
    if ( ::comphelper::findValue( aSupported, sConnectionService ) == -1 )
    {
        sal_Int32 nLen = aSupported.getLength();
        aSupported.realloc( nLen + 1 );
        aSupported.getArray()[ nLen ] = sConnectionService;
    }

    // outta here
    return aSupported;
}


sal_Bool SAL_CALL OConnectionWrapper::supportsService( const OUString& _rServiceName )
{
    return cppu::supportsService(this, _rServiceName);
}


Any SAL_CALL OConnectionWrapper::queryInterface( const Type& _rType )
{
    osl::MutexGuard aGuard(m_aMutex);
    Any aReturn = OConnection_BASE::queryInterface(_rType);
    return aReturn.hasValue() ? aReturn : (m_xProxyConnection.is() ? m_xProxyConnection->queryAggregation(_rType) : aReturn);
}

Sequence< Type > SAL_CALL OConnectionWrapper::getTypes(  )
{
    osl::MutexGuard aGuard(m_aMutex);
    return ::comphelper::concatSequences(
        OConnection_BASE::getTypes(),
        m_xTypeProvider->getTypes()
    );
}

// css::lang::XUnoTunnel
sal_Int64 SAL_CALL OConnectionWrapper::getSomething( const Sequence< sal_Int8 >& rId )
{
    if (comphelper::isUnoTunnelId<OConnectionWrapper>(rId))
        return comphelper::getSomething_cast(this);

    osl::MutexGuard aGuard(m_aMutex);
    if(m_xUnoTunnel.is())
        return m_xUnoTunnel->getSomething(rId);
    return 0;
}


const Sequence< sal_Int8 > & OConnectionWrapper::getUnoTunnelId()
{
    static const comphelper::UnoIdInit implId;
    return implId.getSeq();
}

namespace
{
    class TPropertyValueLessFunctor
    {
    public:
        TPropertyValueLessFunctor()
        {}
        bool operator() (const css::beans::PropertyValue& lhs, const css::beans::PropertyValue& rhs) const
        {
            return lhs.Name.compareToIgnoreAsciiCase(rhs.Name) < 0;
        }
    };

}


// creates a unique id out of the url and sequence of properties
void OConnectionWrapper::createUniqueId( const OUString& _rURL
                    ,Sequence< PropertyValue >& _rInfo
                    ,sal_uInt8* _pBuffer
                    ,const OUString& _rUserName
                    ,const OUString& _rPassword)
{
    // first we create the digest we want to have
    ::comphelper::Hash sha1(::comphelper::HashType::SHA1);
    sha1.update(_rURL.getStr(), _rURL.getLength() * sizeof(sal_Unicode));
    if ( !_rUserName.isEmpty() )
        sha1.update(_rUserName.getStr(), _rUserName.getLength() * sizeof(sal_Unicode));
    if ( !_rPassword.isEmpty() )
        sha1.update(_rPassword.getStr(), _rPassword.getLength() * sizeof(sal_Unicode));
    // now we need to sort the properties
    auto [begin, end] = asNonConstRange(_rInfo);
    std::sort(begin,end,TPropertyValueLessFunctor());

    for (PropertyValue const& prop : _rInfo)
    {
        // we only include strings an integer values
        OUString sValue;
        if ( prop.Value >>= sValue )
            ;
        else
        {
            sal_Int32 nValue = 0;
            if ( prop.Value >>= nValue )
                sValue = OUString::number(nValue);
            else
            {
                Sequence< OUString> aSeq;
                if ( prop.Value >>= aSeq )
                {
                    for (OUString const& s : aSeq)
                        sha1.update(s.getStr(), s.getLength() * sizeof(sal_Unicode));
                }
            }
        }
        if ( !sValue.isEmpty() )
        {
            // we don't have to convert this into UTF8 because we don't store on a file system
            sha1.update(sValue.getStr(), sValue.getLength() * sizeof(sal_Unicode));
        }
    }

    std::vector<unsigned char> result(sha1.finalize());
    std::copy(result.begin(), result.end(), _pBuffer);
}


/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
