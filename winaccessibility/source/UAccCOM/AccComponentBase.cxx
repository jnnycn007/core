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

#include "stdafx.h"
#include "AccComponentBase.h"
#include <com/sun/star/accessibility/XAccessible.hpp>
#include <com/sun/star/accessibility/XAccessibleContext.hpp>
#include <vcl/svapp.hxx>
#include "MAccessible.h"

using namespace com::sun::star::accessibility;
using namespace com::sun::star::uno;

// Construction/Destruction

CAccComponentBase::CAccComponentBase() {}

CAccComponentBase::~CAccComponentBase() {}

/**
 * Returns the location of the upper left corner of the object's bounding
 * box relative to the parent.
 *
 * @param    Location    the upper left corner of the object's bounding box.
 */
COM_DECLSPEC_NOTHROW STDMETHODIMP CAccComponentBase::get_locationInParent(long* x, long* y)
{
    SolarMutexGuard g;

    try
    {
        if (x == nullptr || y == nullptr)
            return E_INVALIDARG;

        if (!pRXComp.is())
            return E_FAIL;

        const css::awt::Point& pt = GetXInterface()->getLocation();
        *x = pt.X;
        *y = pt.Y;
        return S_OK;
    }
    catch (...)
    {
        return E_FAIL;
    }
}

/**
 * Returns the location of the upper left corner of the object's bounding
 * box in screen.
 *
 * @param    Location    the upper left corner of the object's bounding
 *                       box in screen coordinates.
 */
COM_DECLSPEC_NOTHROW STDMETHODIMP CAccComponentBase::get_locationOnScreen(long* x, long* y)
{
    SolarMutexGuard g;

    try
    {
        if (x == nullptr || y == nullptr)
            return E_INVALIDARG;

        if (!pRXComp.is())
            return E_FAIL;

        const css::awt::Point& pt = GetXInterface()->getLocationOnScreen();
        *x = pt.X;
        *y = pt.Y;
        return S_OK;
    }
    catch (...)
    {
        return E_FAIL;
    }
}

/**
 * Grabs the focus to this object.
 *
 * @param    success    the boolean result to be returned.
 */
COM_DECLSPEC_NOTHROW STDMETHODIMP CAccComponentBase::grabFocus(boolean* success)
{
    SolarMutexGuard g;

    try
    {
        if (success == nullptr)
            return E_INVALIDARG;

        if (!pRXComp.is())
        {
            return E_FAIL;
        }
        GetXInterface()->grabFocus();
        *success = TRUE;

        return S_OK;
    }
    catch (...)
    {
        return E_FAIL;
    }
}

/**
 * Returns the foreground color of this object.
 *
 * @param    Color    the color of foreground.
 */
COM_DECLSPEC_NOTHROW STDMETHODIMP CAccComponentBase::get_foreground(IA2Color* foreground)
{
    SolarMutexGuard g;

    try
    {
        if (foreground == nullptr)
            return E_INVALIDARG;

        if (!pRXComp.is())
        {
            return E_FAIL;
        }
        *foreground = static_cast<long>(GetXInterface()->getForeground());

        return S_OK;
    }
    catch (...)
    {
        return E_FAIL;
    }
}

/**
 * Returns the background color of this object.
 *
 * @param    Color    the color of background.
 */
COM_DECLSPEC_NOTHROW STDMETHODIMP CAccComponentBase::get_background(IA2Color* background)
{
    SolarMutexGuard g;

    try
    {
        if (background == nullptr)
            return E_INVALIDARG;

        if (!pRXComp.is())
        {
            return E_FAIL;
        }
        *background = static_cast<long>(GetXInterface()->getBackground());

        return S_OK;
    }
    catch (...)
    {
        return E_FAIL;
    }
}

/**
 * Override of IUNOXWrapper.
 *
 * @param    pXInterface    the pointer of UNO interface.
 */
COM_DECLSPEC_NOTHROW STDMETHODIMP CAccComponentBase::put_XInterface(hyper pXInterface)
{
    // internal IUNOXWrapper - no mutex meeded

    try
    {
        CUNOXWrapper::put_XInterface(pXInterface);
        //special query.
        if (pUNOInterface == nullptr)
            return E_FAIL;
        Reference<XAccessibleContext> pRContext = pUNOInterface->getAccessibleContext();
        if (!pRContext.is())
        {
            return E_FAIL;
        }
        Reference<XAccessibleComponent> pRXI(pRContext, UNO_QUERY);
        if (!pRXI.is())
            pRXComp = nullptr;
        else
            pRXComp = pRXI.get();

        return S_OK;
    }
    catch (...)
    {
        return E_FAIL;
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
