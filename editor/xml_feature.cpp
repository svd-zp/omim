#include "editor/xml_feature.hpp"

#include "base/assert.hpp"
#include "base/macros.hpp"
#include "base/string_utils.hpp"
#include "base/timer.hpp"

#include "geometry/mercator.hpp"

#include "std/set.hpp"
#include "std/unordered_set.hpp"

#include <boost/functional/hash.hpp>

#include "3party/pugixml/src/pugixml.hpp"

namespace
{
auto constexpr kLatLonTolerance = 7;

pugi::xml_node FindTag(pugi::xml_document const & document, string const & key)
{
  return document.select_node(("//tag[@k='" + key + "']").data()).node();
}

m2::PointD PointFromLatLon(pugi::xml_node const node)
{
  double lat, lon;
  if (!strings::to_double(node.attribute("lat").value(), lat))
  {
    MYTHROW(editor::XMLFeatureNoLatLonError,
            ("Can't parse lat attribute: " + string(node.attribute("lat").value())));
  }
 if (!strings::to_double(node.attribute("lon").value(), lon))
 {
   MYTHROW(editor::XMLFeatureNoLatLonError,
           ("Can't parse lon attribute: " + string(node.attribute("lon").value())));
 }
  return MercatorBounds::FromLatLon(lat, lon);
}

void ValidateNode(pugi::xml_node const & node)
{
  if (!node)
    MYTHROW(editor::XMLFeatureNoNodeError, ("Document has no node"));

  // Check if point can be parsed. Throws if it's can't.
  UNUSED_VALUE(PointFromLatLon(node));

  if (!node.attribute("timestamp"))
    MYTHROW(editor::XMLFeatureNoTimestampError, ("Node has no timestamp attribute"));
}
} // namespace

namespace editor
{
char const * const XMLFeature::kLastModified = "timestamp";
char const * const XMLFeature::kHouseNumber = "addr:housenumber";
char const * const XMLFeature::kDefaultName = "name";
char const * const XMLFeature::kLocalName = "name:";
char const * const XMLFeature::kDefaultLang =
    StringUtf8Multilang::GetLangByCode(StringUtf8Multilang::DEFAULT_CODE);
;

XMLFeature::XMLFeature()
{
  m_document.append_child("node");
}

XMLFeature::XMLFeature(string const & xml)
{
  m_document.load(xml.data());
  ValidateNode(GetRootNode());
}

XMLFeature::XMLFeature(pugi::xml_document const & xml)
{
  m_document.reset(xml);
  ValidateNode(GetRootNode());
}

XMLFeature::XMLFeature(pugi::xml_node const & xml)
{
  m_document.reset();
  m_document.append_copy(xml);
  ValidateNode(GetRootNode());
}

void XMLFeature::Save(ostream & ost) const
{
  m_document.save(ost, "  ", pugi::format_indent_attributes);
}

m2::PointD XMLFeature::GetCenter() const
{
  return PointFromLatLon(GetRootNode());
}

void XMLFeature::SetCenter(m2::PointD const & mercatorCenter)
{
  SetAttribute("lat", strings::to_string_dac(MercatorBounds::YToLat(mercatorCenter.y),
                                             kLatLonTolerance));
  SetAttribute("lon", strings::to_string_dac(MercatorBounds::XToLon(mercatorCenter.x),
                                             kLatLonTolerance));
}

string XMLFeature::GetName(string const & lang) const
{
  auto const suffix = (lang == kDefaultLang || lang.empty()) ? "" : ":" + lang;
  return GetTagValue("name" + suffix);
}

string XMLFeature::GetName(uint8_t const langCode) const
{
  return GetName(StringUtf8Multilang::GetLangByCode(langCode));
}

void XMLFeature::SetName(string const & name)
{
  SetName(kDefaultLang, name);
}

void XMLFeature::SetName(string const & lang, string const & name)
{
  auto const suffix = (lang == kDefaultLang || lang.empty()) ? "" : ":" + lang;
  SetTagValue("name" + suffix, name);
}

void XMLFeature::SetName(uint8_t const langCode, string const & name)
{
  SetName(StringUtf8Multilang::GetLangByCode(langCode), name);
}

string XMLFeature::GetHouse() const
{
  return GetTagValue(kHouseNumber);
}

void XMLFeature::SetHouse(string const & house)
{
  SetTagValue(kHouseNumber, house);
}

time_t XMLFeature::GetModificationTime() const
{
  return my::StringToTimestamp(GetRootNode().attribute("timestamp").value());
}

void XMLFeature::SetModificationTime(time_t const time)
{
  SetAttribute(kLastModified, my::TimestampToString(time));
}

bool XMLFeature::HasTag(string const & key) const
{
  return FindTag(m_document, key);
}

bool XMLFeature::HasAttribute(string const & key) const
{
  return GetRootNode().attribute(key.data());
}

bool XMLFeature::HasKey(string const & key) const
{
  return HasTag(key) || HasAttribute(key);
}

string XMLFeature::GetTagValue(string const & key) const
{
  auto const tag = FindTag(m_document, key);
  return tag.attribute("v").value();
}

void XMLFeature::SetTagValue(string const & key, string const value)
{
  auto tag = FindTag(m_document, key);
  if (!tag)
  {
    tag = GetRootNode().append_child("tag");
    tag.append_attribute("k") = key.data();
    tag.append_attribute("v") = value.data();
  }
  else
  {
    tag.attribute("v") = value.data();
  }
}

string XMLFeature::GetAttribute(string const & key) const
{
  return GetRootNode().attribute(key.data()).value();
}

void XMLFeature::SetAttribute(string const & key, string const & value)
{
  auto node = HasAttribute(key)
      ? GetRootNode().attribute(key.data())
      : GetRootNode().append_attribute(key.data());

  node.set_value(value.data());
}

pugi::xml_node const XMLFeature::GetRootNode() const
{
  return m_document.child("node");
}

pugi::xml_node XMLFeature::GetRootNode()
{
  return m_document.child("node");
}
} // namespace editor