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


#ifndef INCLUDED_SVX_SOURCE_INC_GRAPHCTLACCESSIBLECONTEXT_HXX
#define INCLUDED_SVX_SOURCE_INC_GRAPHCTLACCESSIBLECONTEXT_HXX

#include <cppuhelper/compbase.hxx>
#include <com/sun/star/uno/Reference.hxx>
#include <com/sun/star/accessibility/XAccessible.hpp>
#include <com/sun/star/accessibility/XAccessibleComponent.hpp>
#include <com/sun/star/accessibility/XAccessibleContext.hpp>
#include <com/sun/star/accessibility/XAccessibleSelection.hpp>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/lang/XServiceName.hpp>
#include <comphelper/OAccessible.hxx>
#include <cppuhelper/basemutex.hxx>
#include <svl/lstner.hxx>

#include <map>

#include <svx/AccessibleShapeTreeInfo.hxx>
#include <svx/IAccessibleViewForwarder.hxx>
#include <svx/AccessibleShape.hxx>

class GraphCtrl;
class SdrObject;
class SdrModel;
class SdrPage;
class SdrView;

/** @descr
        This base class provides an implementation of the
        <code>AccessibleContext</code> service.
*/

class SvxGraphCtrlAccessibleContext final
    : public cppu::ImplInheritanceHelper<comphelper::OAccessible,
                                         css::accessibility::XAccessibleSelection,
                                         css::lang::XServiceInfo, css::lang::XServiceName>,
      public SfxListener,
      public ::accessibility::IAccessibleViewForwarder
{
public:
    friend class GraphCtrl;

    // internal
    SvxGraphCtrlAccessibleContext(GraphCtrl& rRepresentation);

    void Notify( SfxBroadcaster& aBC, const SfxHint& aHint ) override;

    // XAccessibleComponent
    virtual css::uno::Reference< css::accessibility::XAccessible > SAL_CALL getAccessibleAtPoint( const css::awt::Point& rPoint ) override;
    virtual void SAL_CALL grabFocus() override;

    virtual sal_Int32 SAL_CALL getForeground() override;

    virtual sal_Int32 SAL_CALL getBackground() override;

    // XAccessibleContext
    virtual sal_Int64 SAL_CALL getAccessibleChildCount() override;
    virtual css::uno::Reference< css::accessibility::XAccessible> SAL_CALL getAccessibleChild (sal_Int64 nIndex) override;
    virtual css::uno::Reference< css::accessibility::XAccessible> SAL_CALL getAccessibleParent() override;
    virtual sal_Int16 SAL_CALL getAccessibleRole() override;
    virtual OUString SAL_CALL getAccessibleDescription() override;
    virtual OUString SAL_CALL getAccessibleName() override;
    virtual css::uno::Reference< css::accessibility::XAccessibleRelationSet> SAL_CALL getAccessibleRelationSet() override;
    virtual sal_Int64 SAL_CALL getAccessibleStateSet() override;
    virtual css::lang::Locale SAL_CALL getLocale() override;

    // XServiceInfo
    virtual OUString SAL_CALL getImplementationName() override;
    virtual sal_Bool SAL_CALL supportsService (const OUString& sServiceName) override;
    virtual css::uno::Sequence< OUString> SAL_CALL getSupportedServiceNames() override;

    // XServiceName
    virtual OUString SAL_CALL getServiceName() override;

    // XAccessibleSelection
    virtual void SAL_CALL selectAccessibleChild( sal_Int64 nChildIndex ) override;
    virtual sal_Bool SAL_CALL isAccessibleChildSelected( sal_Int64 nChildIndex ) override;
    virtual void SAL_CALL clearAccessibleSelection() override;
    virtual void SAL_CALL selectAllAccessibleChildren() override;
    virtual sal_Int64 SAL_CALL getSelectedAccessibleChildCount() override;
    virtual css::uno::Reference< css::accessibility::XAccessible > SAL_CALL getSelectedAccessibleChild( sal_Int64 nSelectedChildIndex ) override;
    virtual void SAL_CALL deselectAccessibleChild( sal_Int64 nSelectedChildIndex ) override;

    // IAccessibleViewforwarder
    virtual tools::Rectangle GetVisibleArea() const override;
    virtual Point LogicToPixel (const Point& rPoint) const override;
    virtual Size LogicToPixel (const Size& rSize) const override;

    /** This method is used by the graph control to tell the
        accessibility object about a new model and view.
    */
    void setModelAndView (SdrModel* pModel, SdrView* pView);

protected:
    virtual css::awt::Rectangle implGetBounds() override;

private:
    /// @throws css::lang::IndexOutOfBoundsException
    void checkChildIndexOnSelection(sal_Int64 nIndexOfChild );

    virtual void SAL_CALL disposing() final override;

    /// @throws css::uno::RuntimeException
    /// @throws css::lang::IndexOutOfBoundsException
    SdrObject* getSdrObject( sal_Int64 nIndex );

    css::uno::Reference< css::accessibility::XAccessible > getAccessible( const SdrObject* pObj );

    /** Description of this object.  This is not a constant because it can
        be set from the outside.
    */
    OUString msDescription;

    /** Name of this object.
    */
    OUString msName;

    /// map of accessible shapes
    typedef ::std::map< const SdrObject*, rtl::Reference<::accessibility::AccessibleShape> > ShapesMapType;
    ShapesMapType mxShapes;

    GraphCtrl*  mpControl;

    SdrPage* mpPage;
    SdrView* mpView;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
