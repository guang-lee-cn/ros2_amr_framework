# generated from rosidl_generator_py/resource/_idl.py.em
# with input from ros2_robot_middleware:msg/HealthStatus.idl
# generated code does not contain a copyright notice

# This is being done at the module level and not on the instance level to avoid looking
# for the same variable multiple times on each instance. This variable is not supposed to
# change during runtime so it makes sense to only look for it once.
from os import getenv

ros_python_check_fields = getenv('ROS_PYTHON_CHECK_FIELDS', default='')


# Import statements for member types

import builtins  # noqa: E402, I100

import math  # noqa: E402, I100

import rosidl_parser.definition  # noqa: E402, I100


class Metaclass_HealthStatus(type):
    """Metaclass of message 'HealthStatus'."""

    _CREATE_ROS_MESSAGE = None
    _CONVERT_FROM_PY = None
    _CONVERT_TO_PY = None
    _DESTROY_ROS_MESSAGE = None
    _TYPE_SUPPORT = None

    __constants = {
    }

    @classmethod
    def __import_type_support__(cls):
        try:
            from rosidl_generator_py import import_type_support
            module = import_type_support('ros2_robot_middleware')
        except ImportError:
            import logging
            import traceback
            logger = logging.getLogger(
                'ros2_robot_middleware.msg.HealthStatus')
            logger.debug(
                'Failed to import needed modules for type support:\n' +
                traceback.format_exc())
        else:
            cls._CREATE_ROS_MESSAGE = module.create_ros_message_msg__msg__health_status
            cls._CONVERT_FROM_PY = module.convert_from_py_msg__msg__health_status
            cls._CONVERT_TO_PY = module.convert_to_py_msg__msg__health_status
            cls._TYPE_SUPPORT = module.type_support_msg__msg__health_status
            cls._DESTROY_ROS_MESSAGE = module.destroy_ros_message_msg__msg__health_status

    @classmethod
    def __prepare__(cls, name, bases, **kwargs):
        # list constant names here so that they appear in the help text of
        # the message class under "Data and other attributes defined here:"
        # as well as populate each message instance
        return {
        }


class HealthStatus(metaclass=Metaclass_HealthStatus):
    """Message class 'HealthStatus'."""

    __slots__ = [
        '_node_name',
        '_status',
        '_last_seen_s',
        '_timeout_s',
        '_check_fields',
    ]

    _fields_and_field_types = {
        'node_name': 'string',
        'status': 'string',
        'last_seen_s': 'double',
        'timeout_s': 'double',
    }

    # This attribute is used to store an rosidl_parser.definition variable
    # related to the data type of each of the components the message.
    SLOT_TYPES = (
        rosidl_parser.definition.UnboundedString(),  # noqa: E501
        rosidl_parser.definition.UnboundedString(),  # noqa: E501
        rosidl_parser.definition.BasicType('double'),  # noqa: E501
        rosidl_parser.definition.BasicType('double'),  # noqa: E501
    )

    def __init__(self, **kwargs):
        if 'check_fields' in kwargs:
            self._check_fields = kwargs['check_fields']
        else:
            self._check_fields = ros_python_check_fields == '1'
        if self._check_fields:
            assert all('_' + key in self.__slots__ for key in kwargs.keys()), \
                'Invalid arguments passed to constructor: %s' % \
                ', '.join(sorted(k for k in kwargs.keys() if '_' + k not in self.__slots__))
        self.node_name = kwargs.get('node_name', str())
        self.status = kwargs.get('status', str())
        self.last_seen_s = kwargs.get('last_seen_s', float())
        self.timeout_s = kwargs.get('timeout_s', float())

    def __repr__(self):
        typename = self.__class__.__module__.split('.')
        typename.pop()
        typename.append(self.__class__.__name__)
        args = []
        for s, t in zip(self.get_fields_and_field_types().keys(), self.SLOT_TYPES):
            field = getattr(self, s)
            fieldstr = repr(field)
            # We use Python array type for fields that can be directly stored
            # in them, and "normal" sequences for everything else.  If it is
            # a type that we store in an array, strip off the 'array' portion.
            if (
                isinstance(t, rosidl_parser.definition.AbstractSequence) and
                isinstance(t.value_type, rosidl_parser.definition.BasicType) and
                t.value_type.typename in ['float', 'double', 'int8', 'uint8', 'int16', 'uint16', 'int32', 'uint32', 'int64', 'uint64']
            ):
                if len(field) == 0:
                    fieldstr = '[]'
                else:
                    if self._check_fields:
                        assert fieldstr.startswith('array(')
                    prefix = "array('X', "
                    suffix = ')'
                    fieldstr = fieldstr[len(prefix):-len(suffix)]
            args.append(s + '=' + fieldstr)
        return '%s(%s)' % ('.'.join(typename), ', '.join(args))

    def __eq__(self, other):
        if not isinstance(other, self.__class__):
            return False
        if self.node_name != other.node_name:
            return False
        if self.status != other.status:
            return False
        if self.last_seen_s != other.last_seen_s:
            return False
        if self.timeout_s != other.timeout_s:
            return False
        return True

    @classmethod
    def get_fields_and_field_types(cls):
        from copy import copy
        return copy(cls._fields_and_field_types)

    @builtins.property
    def node_name(self):
        """Message field 'node_name'."""
        return self._node_name

    @node_name.setter
    def node_name(self, value):
        if self._check_fields:
            assert \
                isinstance(value, str), \
                "The 'node_name' field must be of type 'str'"
        self._node_name = value

    @builtins.property
    def status(self):
        """Message field 'status'."""
        return self._status

    @status.setter
    def status(self, value):
        if self._check_fields:
            assert \
                isinstance(value, str), \
                "The 'status' field must be of type 'str'"
        self._status = value

    @builtins.property
    def last_seen_s(self):
        """Message field 'last_seen_s'."""
        return self._last_seen_s

    @last_seen_s.setter
    def last_seen_s(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'last_seen_s' field must be of type 'float'"
            assert not (value < -1.7976931348623157e+308 or value > 1.7976931348623157e+308) or math.isinf(value), \
                "The 'last_seen_s' field must be a double in [-1.7976931348623157e+308, 1.7976931348623157e+308]"
        self._last_seen_s = value

    @builtins.property
    def timeout_s(self):
        """Message field 'timeout_s'."""
        return self._timeout_s

    @timeout_s.setter
    def timeout_s(self, value):
        if self._check_fields:
            assert \
                isinstance(value, float), \
                "The 'timeout_s' field must be of type 'float'"
            assert not (value < -1.7976931348623157e+308 or value > 1.7976931348623157e+308) or math.isinf(value), \
                "The 'timeout_s' field must be a double in [-1.7976931348623157e+308, 1.7976931348623157e+308]"
        self._timeout_s = value
