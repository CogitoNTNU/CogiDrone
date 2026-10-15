# generated from rosidl_generator_py/resource/_idl.py.em
# with input from service_msgs:msg/ServiceEventInfo.idl
# generated code does not contain a copyright notice

# This is being done at the module level and not on the instance level to avoid looking
# for the same variable multiple times on each instance. This variable is not supposed to
# change during runtime so it makes sense to only look for it once.
from os import getenv

ros_python_check_fields = getenv('ROS_PYTHON_CHECK_FIELDS', default='')


# Import statements for member types

import builtins  # noqa: E402, I100

# Member 'client_gid'
import numpy  # noqa: E402, I100

import rosidl_parser.definition  # noqa: E402, I100


class Metaclass_ServiceEventInfo(type):
    """Metaclass of message 'ServiceEventInfo'."""

    _CREATE_ROS_MESSAGE = None
    _CONVERT_FROM_PY = None
    _CONVERT_TO_PY = None
    _DESTROY_ROS_MESSAGE = None
    _TYPE_SUPPORT = None

    __constants = {
        'REQUEST_SENT': 0,
        'REQUEST_RECEIVED': 1,
        'RESPONSE_SENT': 2,
        'RESPONSE_RECEIVED': 3,
    }

    @classmethod
    def __import_type_support__(cls):
        try:
            from rosidl_generator_py import import_type_support
            module = import_type_support('service_msgs')
        except ImportError:
            import logging
            import traceback
            logger = logging.getLogger(
                'service_msgs.msg.ServiceEventInfo')
            logger.debug(
                'Failed to import needed modules for type support:\n' +
                traceback.format_exc())
        else:
            cls._CREATE_ROS_MESSAGE = module.create_ros_message_msg__msg__service_event_info
            cls._CONVERT_FROM_PY = module.convert_from_py_msg__msg__service_event_info
            cls._CONVERT_TO_PY = module.convert_to_py_msg__msg__service_event_info
            cls._TYPE_SUPPORT = module.type_support_msg__msg__service_event_info
            cls._DESTROY_ROS_MESSAGE = module.destroy_ros_message_msg__msg__service_event_info

            from builtin_interfaces.msg import Time
            if Time.__class__._TYPE_SUPPORT is None:
                Time.__class__.__import_type_support__()

    @classmethod
    def __prepare__(cls, name, bases, **kwargs):
        # list constant names here so that they appear in the help text of
        # the message class under "Data and other attributes defined here:"
        # as well as populate each message instance
        return {
            'REQUEST_SENT': cls.__constants['REQUEST_SENT'],
            'REQUEST_RECEIVED': cls.__constants['REQUEST_RECEIVED'],
            'RESPONSE_SENT': cls.__constants['RESPONSE_SENT'],
            'RESPONSE_RECEIVED': cls.__constants['RESPONSE_RECEIVED'],
        }

    @property
    def REQUEST_SENT(self):
        """Message constant 'REQUEST_SENT'."""
        return Metaclass_ServiceEventInfo.__constants['REQUEST_SENT']

    @property
    def REQUEST_RECEIVED(self):
        """Message constant 'REQUEST_RECEIVED'."""
        return Metaclass_ServiceEventInfo.__constants['REQUEST_RECEIVED']

    @property
    def RESPONSE_SENT(self):
        """Message constant 'RESPONSE_SENT'."""
        return Metaclass_ServiceEventInfo.__constants['RESPONSE_SENT']

    @property
    def RESPONSE_RECEIVED(self):
        """Message constant 'RESPONSE_RECEIVED'."""
        return Metaclass_ServiceEventInfo.__constants['RESPONSE_RECEIVED']


class ServiceEventInfo(metaclass=Metaclass_ServiceEventInfo):
    """
    Message class 'ServiceEventInfo'.

    Constants:
      REQUEST_SENT
      REQUEST_RECEIVED
      RESPONSE_SENT
      RESPONSE_RECEIVED
    """

    __slots__ = [
        '_event_type',
        '_stamp',
        '_client_gid',
        '_sequence_number',
        '_check_fields',
    ]

    _fields_and_field_types = {
        'event_type': 'uint8',
        'stamp': 'builtin_interfaces/Time',
        'client_gid': 'uint8[16]',
        'sequence_number': 'int64',
    }

    # This attribute is used to store an rosidl_parser.definition variable
    # related to the data type of each of the components the message.
    SLOT_TYPES = (
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.NamespacedType(['builtin_interfaces', 'msg'], 'Time'),  # noqa: E501
        rosidl_parser.definition.Array(rosidl_parser.definition.BasicType('uint8'), 16),  # noqa: E501
        rosidl_parser.definition.BasicType('int64'),  # noqa: E501
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
        self.event_type = kwargs.get('event_type', int())
        from builtin_interfaces.msg import Time
        self.stamp = kwargs.get('stamp', Time())
        if 'client_gid' not in kwargs:
            self.client_gid = numpy.zeros(16, dtype=numpy.uint8)
        else:
            self.client_gid = kwargs.get('client_gid')
        self.sequence_number = kwargs.get('sequence_number', int())

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
        if self.event_type != other.event_type:
            return False
        if self.stamp != other.stamp:
            return False
        if any(self.client_gid != other.client_gid):
            return False
        if self.sequence_number != other.sequence_number:
            return False
        return True

    @classmethod
    def get_fields_and_field_types(cls):
        from copy import copy
        return copy(cls._fields_and_field_types)

    @builtins.property
    def event_type(self):
        """Message field 'event_type'."""
        return self._event_type

    @event_type.setter
    def event_type(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'event_type' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'event_type' field must be an unsigned integer in [0, 255]"
        self._event_type = value

    @builtins.property
    def stamp(self):
        """Message field 'stamp'."""
        return self._stamp

    @stamp.setter
    def stamp(self, value):
        if self._check_fields:
            from builtin_interfaces.msg import Time
            assert \
                isinstance(value, Time), \
                "The 'stamp' field must be a sub message of type 'Time'"
        self._stamp = value

    @builtins.property
    def client_gid(self):
        """Message field 'client_gid'."""
        return self._client_gid

    @client_gid.setter
    def client_gid(self, value):
        if self._check_fields:
            if isinstance(value, numpy.ndarray):
                assert value.dtype == numpy.uint8, \
                    "The 'client_gid' numpy.ndarray() must have the dtype of 'numpy.uint8'"
                assert value.size == 16, \
                    "The 'client_gid' numpy.ndarray() must have a size of 16"
                self._client_gid = value
                return
            from collections.abc import Sequence
            from collections.abc import Set
            from collections import UserList
            from collections import UserString
            assert \
                ((isinstance(value, Sequence) or
                  isinstance(value, Set) or
                  isinstance(value, UserList)) and
                 not isinstance(value, str) and
                 not isinstance(value, UserString) and
                 len(value) == 16 and
                 all(isinstance(v, int) for v in value) and
                 all(val >= 0 and val < 256 for val in value)), \
                "The 'client_gid' field must be a set or sequence with length 16 and each value of type 'int' and each unsigned integer in [0, 255]"
        self._client_gid = numpy.array(value, dtype=numpy.uint8)

    @builtins.property
    def sequence_number(self):
        """Message field 'sequence_number'."""
        return self._sequence_number

    @sequence_number.setter
    def sequence_number(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'sequence_number' field must be of type 'int'"
            assert value >= -9223372036854775808 and value < 9223372036854775808, \
                "The 'sequence_number' field must be an integer in [-9223372036854775808, 9223372036854775807]"
        self._sequence_number = value
