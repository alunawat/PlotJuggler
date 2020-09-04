#pragma once

#include "rclcpp/rclcpp.hpp"
#include "rmw/rmw.h"
#include "rmw/types.h"
#include "rosbag2_cpp/typesupport_helpers.hpp"
#include "rosidl_typesupport_introspection_cpp/message_introspection.hpp"
#include "ros2_introspection/ros2_introspection.hpp"

#include "PlotJuggler/plotdata.h"

//----------------------------------

enum LargeArrayPolicy : bool
{
  DISCARD_LARGE_ARRAYS = true,
  KEEP_LARGE_ARRAYS = false
};

using SerializedMessage = rcutils_uint8_array_t;


struct TopicInfo{
  std::string name;
  std::string type;
  const rosidl_message_type_support_t* type_support;
};

class MessageParserBase
{
public:
  MessageParserBase(const std::string& topic_name, PlotDataMapRef& plot_data)
    : _use_header_stamp(false), _topic_name(topic_name), _plot_data(plot_data)
  {
  }

  virtual ~MessageParserBase() = default;

  virtual void setUseHeaderStamp(bool use);

  virtual void setMaxArrayPolicy(LargeArrayPolicy policy, size_t max_size)
  {
  }

  virtual bool parseMessage(const SerializedMessage* serialized_msg, double timestamp) = 0;

  static PlotData& getSeries(PlotDataMapRef& plot_data, const std::string key);

  virtual const rosidl_message_type_support_t* typeSupport() const = 0;

protected:
  bool _use_header_stamp;
  const std::string _topic_name;
  PlotDataMapRef& _plot_data;
};

template <typename T>
class BuiltinMessageParser : public MessageParserBase
{
public:
  BuiltinMessageParser(const std::string& topic_name, PlotDataMapRef& plot_data)
    : MessageParserBase(topic_name, plot_data)
  {
    _type_support = rosidl_typesupport_cpp::get_message_type_support_handle<T>();
  }

  virtual bool parseMessage(const SerializedMessage* serialized_msg, double timestamp) override
  {
    T msg;
    if (RMW_RET_OK != rmw_deserialize(serialized_msg, _type_support, &msg))
    {
      throw std::runtime_error("failed to deserialize message");
    }
    parseMessageImpl(msg, timestamp);
    return true;
  }

  virtual void parseMessageImpl(const T& msg, double timestamp) = 0;

  const rosidl_message_type_support_t* typeSupport() const override
  {
    return _type_support;
  }

protected:
  const rosidl_message_type_support_t* _type_support;
};

class IntrospectionParser : public MessageParserBase
{
public:
  IntrospectionParser(const std::string& topic_name, const std::string& topic_type, PlotDataMapRef& plot_data)
    : MessageParserBase(topic_name, plot_data), _intropection_parser(topic_name, topic_type)
  {
  }

  void setMaxArrayPolicy(LargeArrayPolicy policy, size_t max_size) override;

  virtual bool parseMessage(const SerializedMessage* serialized_msg, double timestamp) override;

  const rosidl_message_type_support_t* typeSupport() const override
  {
    return _intropection_parser.topicInfo().type_support;
  }

private:
  Ros2Introspection::Parser _intropection_parser;
  Ros2Introspection::FlatMessage _flat_msg;
  Ros2Introspection::RenamedValues _renamed;
};

class CompositeParser
{
public:
  CompositeParser(PlotDataMapRef& plot_data);

  virtual void setUseHeaderStamp(bool use);

  virtual void setMaxArrayPolicy(LargeArrayPolicy policy, size_t max_size);

  void registerMessageType(const std::string& topic_name, const std::string& topic_type);

  bool parseMessage(const std::string& topic_name, const SerializedMessage* serialized_msg, double timestamp);

  const rosidl_message_type_support_t* typeSupport(const std::string& topic_name) const;

private:
  std::unordered_map<std::string, std::shared_ptr<MessageParserBase>> _parsers;

  LargeArrayPolicy _discard_policy;

  size_t _max_array_size;

  bool _use_header_stamp;

  PlotDataMapRef& _plot_data;
};
