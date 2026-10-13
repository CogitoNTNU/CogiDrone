# generated from rosidl_generator_py/resource/_idl.py.em
# with input from type_description_interfaces:srv/GetTypeDescription.idl
# generated code does not contain a copyright notice

# This is being done at the module level and not on the instance level to avoid looking
# for the same variable multiple times on each instance. This variable is not supposed to
# change during runtime so it makes sense to only look for it once.
from os import getenv

ros_python_check_fields = getenv('ROS_PYTHON_CHECK_FIELDS', default='')


# Import statements for member types

import builtins  # noqa: E402, I100

import rosidl_parser.definition  # noqa: E402, I100


class Metaclass_GetTypeDescription_Request(type):
    """Metaclass of message 'GetTypeDescription_Request'."""

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
            module = import_type_support('type_description_interfaces')
        except ImportError:
            import logging
            import traceback
            logger = logging.getLogger(
                'type_description_interfaces.srv.GetTypeDescription_Request')
            logger.debug(
                'Failed to import needed modules for type support:\n' +
                traceback.format_exc())
        else:
            cls._CREATE_ROS_MESSAGE = module.create_ros_message_msg__srv__get_type_description__request
            cls._CONVERT_FROM_PY = module.convert_from_py_msg__srv__get_type_description__request
            cls._CONVERT_TO_PY = module.convert_to_py_msg__srv__get_type_description__request
            cls._TYPE_SUPPORT = module.type_support_msg__srv__get_type_description__request
            cls._DESTROY_ROS_MESSAGE = module.destroy_ros_message_msg__srv__get_type_description__request

    @classmethod
    def __prepare__(cls, name, bases, **kwargs):
        # list constant names here so that they appear in the help text of
        # the message class under "Data and other attributes defined here:"
        # as well as populate each message instance
        return {
            'INCLUDE_TYPE_SOURCES__DEFAULT': True,
        }

    @property
    def INCLUDE_TYPE_SOURCES__DEFAULT(cls):
        """Return default value for message field 'include_type_sources'."""
        return True


class GetTypeDescription_Request(metaclass=Metaclass_GetTypeDescription_Request):
    """Message class 'GetTypeDescription_Request'."""

    __slots__ = [
        '_type_name',
        '_type_hash',
        '_include_type_sources',
        '_check_fields',
    ]

    _fields_and_field_types = {
        'type_name': 'string',
        'type_hash': 'string',
        'include_type_sources': 'boolean',
    }

    # This attribute is used to store an rosidl_parser.definition variable
    # related to the data type of each of the components the message.
    SLOT_TYPES = (
        rosidl_parser.definition.UnboundedString(),  # noqa: E501
        rosidl_parser.definition.UnboundedString(),  # noqa: E501
        rosidl_parser.definition.BasicType('boolean'),  # noqa: E501
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
        self.type_name = kwargs.get('type_name', str())
        self.type_hash = kwargs.get('type_hash', str())
        self.include_type_sources = kwargs.get(
            'include_type_sources', GetTypeDescription_Request.INCLUDE_TYPE_SOURCES__DEFAULT)

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
        if self.type_name != other.type_name:
            return False
        if self.type_hash != other.type_hash:
            return False
        if self.include_type_sources != other.include_type_sources:
            return False
        return True

    @classmethod
    def get_fields_and_field_types(cls):
        from copy import copy
        return copy(cls._fields_and_field_types)

    @builtins.property
    def type_name(self):
        """Message field 'type_name'."""
        return self._type_name

    @type_name.setter
    def type_name(self, value):
        if self._check_fields:
            assert \
                isinstance(value, str), \
                "The 'type_name' field must be of type 'str'"
        self._type_name = value

    @builtins.property
    def type_hash(self):
        """Message field 'type_hash'."""
        return self._type_hash

    @type_hash.setter
    def type_hash(self, value):
        if self._check_fields:
            assert \
                isinstance(value, str), \
                "The 'type_hash' field must be of type 'str'"
        self._type_hash = value

    @builtins.property
    def include_type_sources(self):
        """Message field 'include_type_sources'."""
        return self._include_type_sources

    @include_type_sources.setter
    def include_type_sources(self, value):
        if self._check_fields:
            assert \
                isinstance(value, bool), \
                "The 'include_type_sources' field must be of type 'bool'"
        self._include_type_sources = value


# Import statements for member types

# already imported above
# import builtins

# already imported above
# import rosidl_parser.definition


class Metaclass_GetTypeDescription_Response(type):
    """Metaclass of message 'GetTypeDescription_Response'."""

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
            module = import_type_support('type_description_interfaces')
        except ImportError:
            import logging
            import traceback
            logger = logging.getLogger(
                'type_description_interfaces.srv.GetTypeDescription_Response')
            logger.debug(
                'Failed to import needed modules for type support:\n' +
                traceback.format_exc())
        else:
            cls._CREATE_ROS_MESSAGE = module.create_ros_message_msg__srv__get_type_description__response
            cls._CONVERT_FROM_PY = module.convert_from_py_msg__srv__get_type_description__response
            cls._CONVERT_TO_PY = module.convert_to_py_msg__srv__get_type_description__response
            cls._TYPE_SUPPORT = module.type_support_msg__srv__get_type_description__response
            cls._DESTROY_ROS_MESSAGE = module.destroy_ros_message_msg__srv__get_type_description__response

            from type_description_interfaces.msg import KeyValue
            if KeyValue.__class__._TYPE_SUPPORT is None:
                KeyValue.__class__.__import_type_support__()

            from type_description_interfaces.msg import TypeDescription
            if TypeDescription.__class__._TYPE_SUPPORT is None:
                TypeDescription.__class__.__import_type_support__()

            from type_description_interfaces.msg import TypeSource
            if TypeSource.__class__._TYPE_SUPPORT is None:
                TypeSource.__class__.__import_type_support__()

    @classmethod
    def __prepare__(cls, name, bases, **kwargs):
        # list constant names here so that they appear in the help text of
        # the message class under "Data and other attributes defined here:"
        # as well as populate each message instance
        return {
        }


class GetTypeDescription_Response(metaclass=Metaclass_GetTypeDescription_Response):
    """Message class 'GetTypeDescription_Response'."""

    __slots__ = [
        '_successful',
        '_failure_reason',
        '_type_description',
        '_type_sources',
        '_extra_information',
        '_check_fields',
    ]

    _fields_and_field_types = {
        'successful': 'boolean',
        'failure_reason': 'string',
        'type_description': 'type_description_interfaces/TypeDescription',
        'type_sources': 'sequence<type_description_interfaces/TypeSource>',
        'extra_information': 'sequence<type_description_interfaces/KeyValue>',
    }

    # This attribute is used to store an rosidl_parser.definition variable
    # related to the data type of each of the components the message.
    SLOT_TYPES = (
        rosidl_parser.definition.BasicType('boolean'),  # noqa: E501
        rosidl_parser.definition.UnboundedString(),  # noqa: E501
        rosidl_parser.definition.NamespacedType(['type_description_interfaces', 'msg'], 'TypeDescription'),  # noqa: E501
        rosidl_parser.definition.UnboundedSequence(rosidl_parser.definition.NamespacedType(['type_description_interfaces', 'msg'], 'TypeSource')),  # noqa: E501
        rosidl_parser.definition.UnboundedSequence(rosidl_parser.definition.NamespacedType(['type_description_interfaces', 'msg'], 'KeyValue')),  # noqa: E501
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
        self.successful = kwargs.get('successful', bool())
        self.failure_reason = kwargs.get('failure_reason', str())
        from type_description_interfaces.msg import TypeDescription
        self.type_description = kwargs.get('type_description', TypeDescription())
        self.type_sources = kwargs.get('type_sources', [])
        self.extra_information = kwargs.get('extra_information', [])

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
        if self.successful != other.successful:
            return False
        if self.failure_reason != other.failure_reason:
            return False
        if self.type_description != other.type_description:
            return False
        if self.type_sources != other.type_sources:
            return False
        if self.extra_information != other.extra_information:
            return False
        return True

    @classmethod
    def get_fields_and_field_types(cls):
        from copy import copy
        return copy(cls._fields_and_field_types)

    @builtins.property
    def successful(self):
        """Message field 'successful'."""
        return self._successful

    @successful.setter
    def successful(self, value):
        if self._check_fields:
            assert \
                isinstance(value, bool), \
                "The 'successful' field must be of type 'bool'"
        self._successful = value

    @builtins.property
    def failure_reason(self):
        """Message field 'failure_reason'."""
        return self._failure_reason

    @failure_reason.setter
    def failure_reason(self, value):
        if self._check_fields:
            assert \
                isinstance(value, str), \
                "The 'failure_reason' field must be of type 'str'"
        self._failure_reason = value

    @builtins.property
    def type_description(self):
        """Message field 'type_description'."""
        return self._type_description

    @type_description.setter
    def type_description(self, value):
        if self._check_fields:
            from type_description_interfaces.msg import TypeDescription
            assert \
                isinstance(value, TypeDescription), \
                "The 'type_description' field must be a sub message of type 'TypeDescription'"
        self._type_description = value

    @builtins.property
    def type_sources(self):
        """Message field 'type_sources'."""
        return self._type_sources

    @type_sources.setter
    def type_sources(self, value):
        if self._check_fields:
            from type_description_interfaces.msg import TypeSource
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
                 all(isinstance(v, TypeSource) for v in value) and
                 True), \
                "The 'type_sources' field must be a set or sequence and each value of type 'TypeSource'"
        self._type_sources = value

    @builtins.property
    def extra_information(self):
        """Message field 'extra_information'."""
        return self._extra_information

    @extra_information.setter
    def extra_information(self, value):
        if self._check_fields:
            from type_description_interfaces.msg import KeyValue
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
                 all(isinstance(v, KeyValue) for v in value) and
                 True), \
                "The 'extra_information' field must be a set or sequence and each value of type 'KeyValue'"
        self._extra_information = value


# Import statements for member types

# already imported above
# import builtins

# already imported above
# import rosidl_parser.definition


class Metaclass_GetTypeDescription_Event(type):
    """Metaclass of message 'GetTypeDescription_Event'."""

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
            module = import_type_support('type_description_interfaces')
        except ImportError:
            import logging
            import traceback
            logger = logging.getLogger(
                'type_description_interfaces.srv.GetTypeDescription_Event')
            logger.debug(
                'Failed to import needed modules for type support:\n' +
                traceback.format_exc())
        else:
            cls._CREATE_ROS_MESSAGE = module.create_ros_message_msg__srv__get_type_description__event
            cls._CONVERT_FROM_PY = module.convert_from_py_msg__srv__get_type_description__event
            cls._CONVERT_TO_PY = module.convert_to_py_msg__srv__get_type_description__event
            cls._TYPE_SUPPORT = module.type_support_msg__srv__get_type_description__event
            cls._DESTROY_ROS_MESSAGE = module.destroy_ros_message_msg__srv__get_type_description__event

            from service_msgs.msg import ServiceEventInfo
            if ServiceEventInfo.__class__._TYPE_SUPPORT is None:
                ServiceEventInfo.__class__.__import_type_support__()

    @classmethod
    def __prepare__(cls, name, bases, **kwargs):
        # list constant names here so that they appear in the help text of
        # the message class under "Data and other attributes defined here:"
        # as well as populate each message instance
        return {
        }


class GetTypeDescription_Event(metaclass=Metaclass_GetTypeDescription_Event):
    """Message class 'GetTypeDescription_Event'."""

    __slots__ = [
        '_info',
        '_request',
        '_response',
        '_check_fields',
    ]

    _fields_and_field_types = {
        'info': 'service_msgs/ServiceEventInfo',
        'request': 'sequence<type_description_interfaces/GetTypeDescription_Request, 1>',
        'response': 'sequence<type_description_interfaces/GetTypeDescription_Response, 1>',
    }

    # This attribute is used to store an rosidl_parser.definition variable
    # related to the data type of each of the components the message.
    SLOT_TYPES = (
        rosidl_parser.definition.NamespacedType(['service_msgs', 'msg'], 'ServiceEventInfo'),  # noqa: E501
        rosidl_parser.definition.BoundedSequence(rosidl_parser.definition.NamespacedType(['type_description_interfaces', 'srv'], 'GetTypeDescription_Request'), 1),  # noqa: E501
        rosidl_parser.definition.BoundedSequence(rosidl_parser.definition.NamespacedType(['type_description_interfaces', 'srv'], 'GetTypeDescription_Response'), 1),  # noqa: E501
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
        from service_msgs.msg import ServiceEventInfo
        self.info = kwargs.get('info', ServiceEventInfo())
        self.request = kwargs.get('request', [])
        self.response = kwargs.get('response', [])

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
        if self.info != other.info:
            return False
        if self.request != other.request:
            return False
        if self.response != other.response:
            return False
        return True

    @classmethod
    def get_fields_and_field_types(cls):
        from copy import copy
        return copy(cls._fields_and_field_types)

    @builtins.property
    def info(self):
        """Message field 'info'."""
        return self._info

    @info.setter
    def info(self, value):
        if self._check_fields:
            from service_msgs.msg import ServiceEventInfo
            assert \
                isinstance(value, ServiceEventInfo), \
                "The 'info' field must be a sub message of type 'ServiceEventInfo'"
        self._info = value

    @builtins.property
    def request(self):
        """Message field 'request'."""
        return self._request

    @request.setter
    def request(self, value):
        if self._check_fields:
            from type_description_interfaces.srv import GetTypeDescription_Request
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
                 len(value) <= 1 and
                 all(isinstance(v, GetTypeDescription_Request) for v in value) and
                 True), \
                "The 'request' field must be a set or sequence with length <= 1 and each value of type 'GetTypeDescription_Request'"
        self._request = value

    @builtins.property
    def response(self):
        """Message field 'response'."""
        return self._response

    @response.setter
    def response(self, value):
        if self._check_fields:
            from type_description_interfaces.srv import GetTypeDescription_Response
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
                 len(value) <= 1 and
                 all(isinstance(v, GetTypeDescription_Response) for v in value) and
                 True), \
                "The 'response' field must be a set or sequence with length <= 1 and each value of type 'GetTypeDescription_Response'"
        self._response = value


class Metaclass_GetTypeDescription(type):
    """Metaclass of service 'GetTypeDescription'."""

    _TYPE_SUPPORT = None

    @classmethod
    def __import_type_support__(cls):
        try:
            from rosidl_generator_py import import_type_support
            module = import_type_support('type_description_interfaces')
        except ImportError:
            import logging
            import traceback
            logger = logging.getLogger(
                'type_description_interfaces.srv.GetTypeDescription')
            logger.debug(
                'Failed to import needed modules for type support:\n' +
                traceback.format_exc())
        else:
            cls._TYPE_SUPPORT = module.type_support_srv__srv__get_type_description

            from type_description_interfaces.srv import _get_type_description
            if _get_type_description.Metaclass_GetTypeDescription_Request._TYPE_SUPPORT is None:
                _get_type_description.Metaclass_GetTypeDescription_Request.__import_type_support__()
            if _get_type_description.Metaclass_GetTypeDescription_Response._TYPE_SUPPORT is None:
                _get_type_description.Metaclass_GetTypeDescription_Response.__import_type_support__()
            if _get_type_description.Metaclass_GetTypeDescription_Event._TYPE_SUPPORT is None:
                _get_type_description.Metaclass_GetTypeDescription_Event.__import_type_support__()


class GetTypeDescription(metaclass=Metaclass_GetTypeDescription):
    from type_description_interfaces.srv._get_type_description import GetTypeDescription_Request as Request
    from type_description_interfaces.srv._get_type_description import GetTypeDescription_Response as Response
    from type_description_interfaces.srv._get_type_description import GetTypeDescription_Event as Event

    def __init__(self):
        raise NotImplementedError('Service classes can not be instantiated')
