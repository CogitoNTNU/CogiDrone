# generated from rosidl_generator_py/resource/_idl.py.em
# with input from rcl_interfaces:msg/LoggerLevel.idl
# generated code does not contain a copyright notice

# This is being done at the module level and not on the instance level to avoid looking
# for the same variable multiple times on each instance. This variable is not supposed to
# change during runtime so it makes sense to only look for it once.
from os import getenv

ros_python_check_fields = getenv('ROS_PYTHON_CHECK_FIELDS', default='')


# Import statements for member types

import builtins  # noqa: E402, I100

import rosidl_parser.definition  # noqa: E402, I100


class Metaclass_LoggerLevel(type):
    """Metaclass of message 'LoggerLevel'."""

    _CREATE_ROS_MESSAGE = None
    _CONVERT_FROM_PY = None
    _CONVERT_TO_PY = None
    _DESTROY_ROS_MESSAGE = None
    _TYPE_SUPPORT = None

    __constants = {
        'LOG_LEVEL_UNKNOWN': 0,
        'LOG_LEVEL_DEBUG': 10,
        'LOG_LEVEL_INFO': 20,
        'LOG_LEVEL_WARN': 30,
        'LOG_LEVEL_ERROR': 40,
        'LOG_LEVEL_FATAL': 50,
    }

    @classmethod
    def __import_type_support__(cls):
        try:
            from rosidl_generator_py import import_type_support
            module = import_type_support('rcl_interfaces')
        except ImportError:
            import logging
            import traceback
            logger = logging.getLogger(
                'rcl_interfaces.msg.LoggerLevel')
            logger.debug(
                'Failed to import needed modules for type support:\n' +
                traceback.format_exc())
        else:
            cls._CREATE_ROS_MESSAGE = module.create_ros_message_msg__msg__logger_level
            cls._CONVERT_FROM_PY = module.convert_from_py_msg__msg__logger_level
            cls._CONVERT_TO_PY = module.convert_to_py_msg__msg__logger_level
            cls._TYPE_SUPPORT = module.type_support_msg__msg__logger_level
            cls._DESTROY_ROS_MESSAGE = module.destroy_ros_message_msg__msg__logger_level

    @classmethod
    def __prepare__(cls, name, bases, **kwargs):
        # list constant names here so that they appear in the help text of
        # the message class under "Data and other attributes defined here:"
        # as well as populate each message instance
        return {
            'LOG_LEVEL_UNKNOWN': cls.__constants['LOG_LEVEL_UNKNOWN'],
            'LOG_LEVEL_DEBUG': cls.__constants['LOG_LEVEL_DEBUG'],
            'LOG_LEVEL_INFO': cls.__constants['LOG_LEVEL_INFO'],
            'LOG_LEVEL_WARN': cls.__constants['LOG_LEVEL_WARN'],
            'LOG_LEVEL_ERROR': cls.__constants['LOG_LEVEL_ERROR'],
            'LOG_LEVEL_FATAL': cls.__constants['LOG_LEVEL_FATAL'],
        }

    @property
    def LOG_LEVEL_UNKNOWN(self):
        """Message constant 'LOG_LEVEL_UNKNOWN'."""
        return Metaclass_LoggerLevel.__constants['LOG_LEVEL_UNKNOWN']

    @property
    def LOG_LEVEL_DEBUG(self):
        """Message constant 'LOG_LEVEL_DEBUG'."""
        return Metaclass_LoggerLevel.__constants['LOG_LEVEL_DEBUG']

    @property
    def LOG_LEVEL_INFO(self):
        """Message constant 'LOG_LEVEL_INFO'."""
        return Metaclass_LoggerLevel.__constants['LOG_LEVEL_INFO']

    @property
    def LOG_LEVEL_WARN(self):
        """Message constant 'LOG_LEVEL_WARN'."""
        return Metaclass_LoggerLevel.__constants['LOG_LEVEL_WARN']

    @property
    def LOG_LEVEL_ERROR(self):
        """Message constant 'LOG_LEVEL_ERROR'."""
        return Metaclass_LoggerLevel.__constants['LOG_LEVEL_ERROR']

    @property
    def LOG_LEVEL_FATAL(self):
        """Message constant 'LOG_LEVEL_FATAL'."""
        return Metaclass_LoggerLevel.__constants['LOG_LEVEL_FATAL']


class LoggerLevel(metaclass=Metaclass_LoggerLevel):
    """
    Message class 'LoggerLevel'.

    Constants:
      LOG_LEVEL_UNKNOWN
      LOG_LEVEL_DEBUG
      LOG_LEVEL_INFO
      LOG_LEVEL_WARN
      LOG_LEVEL_ERROR
      LOG_LEVEL_FATAL
    """

    __slots__ = [
        '_name',
        '_level',
        '_check_fields',
    ]

    _fields_and_field_types = {
        'name': 'string',
        'level': 'uint32',
    }

    # This attribute is used to store an rosidl_parser.definition variable
    # related to the data type of each of the components the message.
    SLOT_TYPES = (
        rosidl_parser.definition.UnboundedString(),  # noqa: E501
        rosidl_parser.definition.BasicType('uint32'),  # noqa: E501
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
        self.name = kwargs.get('name', str())
        self.level = kwargs.get('level', int())

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
        if self.name != other.name:
            return False
        if self.level != other.level:
            return False
        return True

    @classmethod
    def get_fields_and_field_types(cls):
        from copy import copy
        return copy(cls._fields_and_field_types)

    @builtins.property
    def name(self):
        """Message field 'name'."""
        return self._name

    @name.setter
    def name(self, value):
        if self._check_fields:
            assert \
                isinstance(value, str), \
                "The 'name' field must be of type 'str'"
        self._name = value

    @builtins.property
    def level(self):
        """Message field 'level'."""
        return self._level

    @level.setter
    def level(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'level' field must be of type 'int'"
            assert value >= 0 and value < 4294967296, \
                "The 'level' field must be an unsigned integer in [0, 4294967295]"
        self._level = value
