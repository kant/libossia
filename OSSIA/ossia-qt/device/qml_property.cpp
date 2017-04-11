#include "qml_property.hpp"
#include <ossia-qt/device/qml_device.hpp>
#include <ossia-qt/device/qml_node.hpp>
#include <ossia-qt/js_utilities.hpp>

#include <ossia/network/generic/generic_device.hpp>
#include <QDebug>

namespace ossia
{
namespace qt
{
void qml_property::resetNode()
{
  m_node.clear();
  if(m_ossia_node)
  {
    auto par = m_ossia_node->getParent();
    if(par)
    {
      auto node = m_ossia_node;
      m_address = nullptr;
      m_ossia_node = nullptr;
      par->removeChild(*node);
    }
  }

  if(m_device)
  {
    std::string node_name;
    bool relative = false;

    if(m_userRequestedNode.isEmpty())
    {
      node_name = m_targetProperty.name().replace('.', '_').toStdString();
      relative = true;
    }
    else if(m_userRequestedNode[0] != '/')
    {
      node_name = m_userRequestedNode.toStdString();
      relative = true;
    }
    else
    {
      node_name = m_userRequestedNode.toStdString();
    }

    ossia::net::node_base& parent =
        relative
        ? findClosestParent(m_targetProperty.object(), m_device->device().getRootNode())
        : m_device->device().getRootNode();

    if(m_device->readPreset())
    {
      m_ossia_node = ossia::net::find_node(parent, node_name);
    }
    else
    {
      m_ossia_node = &ossia::net::create_node(parent, node_name);
    }

    if(m_ossia_node)
    {
      m_ossia_node->aboutToBeDeleted.connect<qml_property, &qml_property::on_node_deleted>(this);
      m_node = QString::fromStdString(m_ossia_node->getName());

      setPath(
            QString::fromStdString(
              ossia::net::address_string_from_node(*m_ossia_node)));
      setupAddress();
      return;
    } // else, we go through the reset:
  }

  // In case something went wrong...
  setPath({});
  m_address = nullptr;
}

qml_property::qml_property(QQuickItem *parent)
  : qml_node_base(parent)
{
  connect(this, &qml_property::setValue_sig,
          this, &qml_property::setValue_slot,
          Qt::QueuedConnection);

  setDevice(&qml_singleton_device::instance());
  resetNode();
}

qml_property::~qml_property()
{
  m_address = nullptr;
}

void qml_property::setTarget(const QQmlProperty &prop)
{
  m_targetProperty = prop;
  resetNode();
}

void qml_property::qtVariantChanged()
{
  if(m_address)
  {
    m_address->setValueQuiet(qt_to_ossia{}(m_targetProperty.read()));
    m_device->device().getProtocol().push(*m_address);
  }
}

void qml_property::setDevice(QObject *device)
{
  auto olddev = m_device;
  auto newdev = qobject_cast<qml_device*>(device);
  if(olddev != newdev)
  {
    qml_node_base::setDevice(device);

    if(olddev)
    {
      olddev->properties.erase(this);
    }

    if(newdev)
    {
      newdev->properties.insert(this);
    }
  }
}

void qml_property::setValue_slot(const value& v)
{
  auto cur = m_targetProperty.read();
  auto next = ossia_to_qvariant{}((QVariant::Type)m_targetProperty.propertyType(), v);
  if(cur != next)
    m_targetProperty.write(next);
}

void qml_property::setupAddress()
{
  m_address = nullptr;
  if(m_ossia_node)
  {
    m_ossia_node->removeAddress();
    m_address = m_ossia_node->createAddress(ossia::val_type::IMPULSE);
    if(m_address)
    {
      set_address_type((QVariant::Type)m_targetProperty.propertyType(), *m_address);

      if(m_targetProperty.hasNotifySignal())
      {
        m_targetProperty.connectNotifySignal(this, SLOT(qtVariantChanged()));
      }

      m_address->add_callback([this] (const ossia::value& v) { setValue_sig(v); });
      m_address->setValueQuiet(qt_to_ossia{}(m_targetProperty.read()));
    }
  }
}

void qml_property::on_node_deleted(const net::node_base&)
{
  m_address = nullptr;
  m_ossia_node = nullptr;
}

}
}
