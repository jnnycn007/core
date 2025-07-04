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
#pragma once

#include <controls/table/AccessibleGridControlBase.hxx>
#include <controls/table/tablecontrol.hxx>

#include <comphelper/accessibletexthelper.hxx>
#include <cppuhelper/implbase2.hxx>
#include <com/sun/star/accessibility/AccessibleScrollType.hpp>

namespace accessibility
{
class AccessibleGridControlCell : public AccessibleGridControlBase
{
private:
    sal_Int32 m_nRowPos; // the row number of the table cell
    sal_Int32 m_nColPos; // the column id of the table cell

protected:
    // attribute access
    sal_Int32 getRowPos() const { return m_nRowPos; }
    sal_Int32 getColumnPos() const { return m_nColPos; }

    // XAccessibleComponent
    virtual void SAL_CALL grabFocus() override;

public:
    // XAccessibleContext
    virtual OUString SAL_CALL getAccessibleName() override;

protected:
    AccessibleGridControlCell(const css::uno::Reference<css::accessibility::XAccessible>& _rxParent,
                              svt::table::TableControl& _rTable, sal_Int32 _nRowPos,
                              sal_uInt16 _nColPos, AccessibleTableControlObjType _eType);

    virtual ~AccessibleGridControlCell() override = default;

private:
    AccessibleGridControlCell(const AccessibleGridControlCell&) = delete;
    AccessibleGridControlCell& operator=(const AccessibleGridControlCell&) = delete;
};

// implementation of a table cell of GridControl
class AccessibleGridControlTableCell final
    : public cppu::ImplInheritanceHelper<AccessibleGridControlCell,
                                         css::accessibility::XAccessibleText>,
      public ::comphelper::OCommonAccessibleText
{
private:
    // OCommonAccessibleText
    virtual OUString implGetText() override;
    virtual css::lang::Locale implGetLocale() override;
    virtual void implGetSelection(sal_Int32& nStartIndex, sal_Int32& nEndIndex) override final;
    virtual AbsoluteScreenPixelRectangle implGetBoundingBoxOnScreen() override;

public:
    AccessibleGridControlTableCell(
        const css::uno::Reference<css::accessibility::XAccessible>& _rxParent,
        svt::table::TableControl& _rTable, sal_Int32 _nRowId, sal_uInt16 _nColId);

    /** @return  The index of this object among the parent's children. */
    virtual sal_Int64 SAL_CALL getAccessibleIndexInParent() override;

    /** @return
                The name of this class.
        */
    virtual OUString SAL_CALL getImplementationName() override;

    /** @return
                The count of visible children.
        */
    virtual sal_Int64 SAL_CALL getAccessibleChildCount() override;

    /** @return
                The XAccessible interface of the specified child.
        */
    virtual css::uno::Reference<css::accessibility::XAccessible>
        SAL_CALL getAccessibleChild(sal_Int64 nChildIndex) override;

    /** Return a bitset of states of the current object.
        */
    sal_Int64 implCreateStateSet() override;

    // XAccessibleText
    virtual sal_Int32 SAL_CALL getCaretPosition() override;
    virtual sal_Bool SAL_CALL setCaretPosition(sal_Int32 nIndex) override;
    virtual sal_Unicode SAL_CALL getCharacter(sal_Int32 nIndex) override;
    virtual css::uno::Sequence<css::beans::PropertyValue> SAL_CALL getCharacterAttributes(
        sal_Int32 nIndex, const css::uno::Sequence<OUString>& aRequestedAttributes) override;
    virtual css::awt::Rectangle SAL_CALL getCharacterBounds(sal_Int32 nIndex) override;
    virtual sal_Int32 SAL_CALL getCharacterCount() override;
    virtual sal_Int32 SAL_CALL getIndexAtPoint(const css::awt::Point& aPoint) override;
    virtual OUString SAL_CALL getSelectedText() override;
    virtual sal_Int32 SAL_CALL getSelectionStart() override;
    virtual sal_Int32 SAL_CALL getSelectionEnd() override;
    virtual sal_Bool SAL_CALL setSelection(sal_Int32 nStartIndex, sal_Int32 nEndIndex) override;
    virtual OUString SAL_CALL getText() override;
    virtual OUString SAL_CALL getTextRange(sal_Int32 nStartIndex, sal_Int32 nEndIndex) override;
    virtual css::accessibility::TextSegment SAL_CALL getTextAtIndex(sal_Int32 nIndex,
                                                                    sal_Int16 aTextType) override;
    virtual css::accessibility::TextSegment SAL_CALL
    getTextBeforeIndex(sal_Int32 nIndex, sal_Int16 aTextType) override;
    virtual css::accessibility::TextSegment SAL_CALL
    getTextBehindIndex(sal_Int32 nIndex, sal_Int16 aTextType) override;
    virtual sal_Bool SAL_CALL copyText(sal_Int32 nStartIndex, sal_Int32 nEndIndex) override;
    virtual sal_Bool SAL_CALL
    scrollSubstringTo(sal_Int32 nStartIndex, sal_Int32 nEndIndex,
                      css::accessibility::AccessibleScrollType aScrollType) override;
};
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
