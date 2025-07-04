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

#include <com/sun/star/uno/Any.hxx>
#include <svl/cenumitm.hxx>
#include <svl/eitem.hxx>
#include <unordered_map>
#include <comphelper/extract.hxx>
#include <libxml/xmlwriter.h>
#include <sal/log.hxx>


// virtual
bool SfxEnumItemInterface::operator ==(const SfxPoolItem & rItem) const
{
    SAL_WARN_IF(!SfxPoolItem::operator ==(rItem), "svl.items", "unequal type, with ID/pos " << Which() );
    return GetEnumValue()
               == static_cast< const SfxEnumItemInterface * >(&rItem)->
                      GetEnumValue();
}

// virtual
bool SfxEnumItemInterface::GetPresentation(SfxItemPresentation, MapUnit,
                                      MapUnit, OUString & rText,
                                      const IntlWrapper&) const
{
    rText = OUString::number( GetEnumValue() );
    return true;
}

// virtual
bool SfxEnumItemInterface::QueryValue(css::uno::Any& rVal, sal_uInt8)
    const
{
    rVal <<= GetEnumValue();
    return true;
}

// virtual
bool SfxEnumItemInterface::PutValue(const css::uno::Any& rVal,
                                    sal_uInt8)
{
    sal_Int32 nTheValue = 0;

    if ( ::cppu::enum2int( nTheValue, rVal ) )
    {
        SetEnumValue(sal_uInt16(nTheValue));
        return true;
    }
    SAL_WARN("svl.items", "SfxEnumItemInterface::PutValue(): Wrong type");
    return false;
}

// virtual
bool SfxEnumItemInterface::HasBoolValue() const
{
    return false;
}

// virtual
bool SfxEnumItemInterface::GetBoolValue() const
{
    return false;
}

// virtual
void SfxEnumItemInterface::SetBoolValue(bool)
{}

typedef std::unordered_map<sal_uInt16, std::pair<const SfxPoolItem*, const SfxPoolItem*>> SfxBoolItemMap;

namespace
{
    class SfxBoolItemInstanceManager : public ItemInstanceManager
    {
        SfxBoolItemMap  maRegistered;

    public:
        SfxBoolItemInstanceManager(SfxItemType aSfxItemType)
        : ItemInstanceManager(aSfxItemType)
        , maRegistered()
        {
        }

    private:
        // standard interface, accessed exclusively
        // by implCreateItemEntry/implCleanupItemEntry
        virtual const SfxPoolItem* find(const SfxPoolItem&) const override;
        virtual void add(const SfxPoolItem&) override;
        virtual void remove(const SfxPoolItem&) override;
    };

    const SfxPoolItem* SfxBoolItemInstanceManager::find(const SfxPoolItem& rItem) const
    {
        SfxBoolItemMap::const_iterator aHit(maRegistered.find(rItem.Which()));
        if (aHit == maRegistered.end())
            return nullptr;

        const SfxBoolItem& rSfxBoolItem(static_cast<const SfxBoolItem&>(rItem));
        if (rSfxBoolItem.GetValue())
            return aHit->second.first;
        return aHit->second.second;
    }

    void SfxBoolItemInstanceManager::add(const SfxPoolItem& rItem)
    {
        SfxBoolItemMap::iterator aHit(maRegistered.find(rItem.Which()));
        const SfxBoolItem& rSfxBoolItem(static_cast<const SfxBoolItem&>(rItem));

        if (aHit == maRegistered.end())
        {
            if (rSfxBoolItem.GetValue())
                maRegistered.insert({rItem.Which(), std::make_pair(&rItem, nullptr)});
            else
                maRegistered.insert({rItem.Which(), std::make_pair(nullptr, &rItem)});
        }
        else
        {
            if (rSfxBoolItem.GetValue())
                aHit->second.first = &rItem;
            else
                aHit->second.second = &rItem;
        }
    }

    void SfxBoolItemInstanceManager::remove(const SfxPoolItem& rItem)
    {
        SfxBoolItemMap::iterator aHit(maRegistered.find(rItem.Which()));
        const SfxBoolItem& rSfxBoolItem(static_cast<const SfxBoolItem&>(rItem));

        if (aHit != maRegistered.end())
        {
            if (rSfxBoolItem.GetValue())
                aHit->second.first = nullptr;
            else
                aHit->second.second = nullptr;

            if (aHit->second.first == nullptr && aHit->second.second == nullptr)
                maRegistered.erase(aHit);
        }
    }
}

ItemInstanceManager* SfxBoolItem::getItemInstanceManager() const
{
    static SfxBoolItemInstanceManager aInstanceManager(ItemType());
    return &aInstanceManager;
}

void SfxBoolItem::SetValue(bool const bTheValue)
{
    if (m_bValue == bTheValue)
        return;

    ASSERT_CHANGE_REFCOUNTED_ITEM;
    m_bValue = bTheValue;
}

SfxPoolItem* SfxBoolItem::CreateDefault()
{
    return new SfxBoolItem();
}

// virtual
bool SfxBoolItem::operator ==(const SfxPoolItem & rItem) const
{
    assert(SfxPoolItem::operator==(rItem));
    return m_bValue == static_cast< SfxBoolItem const * >(&rItem)->m_bValue;
}

// virtual
bool SfxBoolItem::GetPresentation(SfxItemPresentation,
                                                 MapUnit, MapUnit,
                                                 OUString & rText,
                                                 const IntlWrapper&) const
{
    rText = GetValueTextByVal(m_bValue);
    return true;
}

void SfxBoolItem::dumpAsXml(xmlTextWriterPtr pWriter) const
{
    (void)xmlTextWriterStartElement(pWriter, BAD_CAST("SfxBoolItem"));
    (void)xmlTextWriterWriteAttribute(pWriter, BAD_CAST("value"), BAD_CAST(GetValueTextByVal(m_bValue).toUtf8().getStr()));
    SfxPoolItem::dumpAsXml(pWriter);
    (void)xmlTextWriterEndElement(pWriter);
}

// virtual
bool SfxBoolItem::QueryValue(css::uno::Any& rVal, sal_uInt8) const
{
    rVal <<= m_bValue;
    return true;
}

// virtual
bool SfxBoolItem::PutValue(const css::uno::Any& rVal, sal_uInt8)
{
    bool bTheValue = bool();
    if (rVal >>= bTheValue)
    {
        if (m_bValue == bTheValue)
            return true;

        ASSERT_CHANGE_REFCOUNTED_ITEM;
        m_bValue = bTheValue;
        return true;
    }
    SAL_WARN("svl.items", "SfxBoolItem::PutValue(): Wrong type");
    return false;
}

// virtual
SfxBoolItem* SfxBoolItem::Clone(SfxItemPool *) const
{
    return new SfxBoolItem(*this);
}

// virtual
OUString SfxBoolItem::GetValueTextByVal(bool bTheValue) const
{
    return bTheValue ?  u"TRUE"_ustr : u"FALSE"_ustr;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
