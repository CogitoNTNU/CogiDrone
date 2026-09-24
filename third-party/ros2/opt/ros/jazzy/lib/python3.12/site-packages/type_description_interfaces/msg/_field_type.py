# generated from rosidl_generator_py/resource/_idl.py.em
# with input from type_description_interfaces:msg/FieldType.idl
# generated code does not contain a copyright notice

# This is being done at the module level and not on the instance level to avoid looking
# for the same variable multiple times on each instance. This variable is not supposed to
# change during runtime so it makes sense to only look for it once.
from os import getenv

ros_python_check_fields = getenv('ROS_PYTHON_CHECK_FIELDS', default='')


# Import statements for member types

import builtins  # noqa: E402, I100

import rosidl_parser.definition  # noqa: E402, I100


class Metaclass_FieldType(type):
    """Metaclass of message 'FieldType'."""

    _CREATE_ROS_MESSAGE = None
    _CONVERT_FROM_PY = None
    _CONVERT_TO_PY = None
    _DESTROY_ROS_MESSAGE = None
    _TYPE_SUPPORT = None

    __constants = {
        'FIELD_TYPE_NOT_SET': 0,
        'FIELD_TYPE_NESTED_TYPE': 1,
        'FIELD_TYPE_INT8': 2,
        'FIELD_TYPE_UINT8': 3,
        'FIELD_TYPE_INT16': 4,
        'FIELD_TYPE_UINT16': 5,
        'FIELD_TYPE_INT32': 6,
        'FIELD_TYPE_UINT32': 7,
        'FIELD_TYPE_INT64': 8,
        'FIELD_TYPE_UINT64': 9,
        'FIELD_TYPE_FLOAT': 10,
        'FIELD_TYPE_DOUBLE': 11,
        'FIELD_TYPE_LONG_DOUBLE': 12,
        'FIELD_TYPE_CHAR': 13,
        'FIELD_TYPE_WCHAR': 14,
        'FIELD_TYPE_BOOLEAN': 15,
        'FIELD_TYPE_BYTE': 16,
        'FIELD_TYPE_STRING': 17,
        'FIELD_TYPE_WSTRING': 18,
        'FIELD_TYPE_FIXED_STRING': 19,
        'FIELD_TYPE_FIXED_WSTRING': 20,
        'FIELD_TYPE_BOUNDED_STRING': 21,
        'FIELD_TYPE_BOUNDED_WSTRING': 22,
        'FIELD_TYPE_NESTED_TYPE_ARRAY': 49,
        'FIELD_TYPE_INT8_ARRAY': 50,
        'FIELD_TYPE_UINT8_ARRAY': 51,
        'FIELD_TYPE_INT16_ARRAY': 52,
        'FIELD_TYPE_UINT16_ARRAY': 53,
        'FIELD_TYPE_INT32_ARRAY': 54,
        'FIELD_TYPE_UINT32_ARRAY': 55,
        'FIELD_TYPE_INT64_ARRAY': 56,
        'FIELD_TYPE_UINT64_ARRAY': 57,
        'FIELD_TYPE_FLOAT_ARRAY': 58,
        'FIELD_TYPE_DOUBLE_ARRAY': 59,
        'FIELD_TYPE_LONG_DOUBLE_ARRAY': 60,
        'FIELD_TYPE_CHAR_ARRAY': 61,
        'FIELD_TYPE_WCHAR_ARRAY': 62,
        'FIELD_TYPE_BOOLEAN_ARRAY': 63,
        'FIELD_TYPE_BYTE_ARRAY': 64,
        'FIELD_TYPE_STRING_ARRAY': 65,
        'FIELD_TYPE_WSTRING_ARRAY': 66,
        'FIELD_TYPE_FIXED_STRING_ARRAY': 67,
        'FIELD_TYPE_FIXED_WSTRING_ARRAY': 68,
        'FIELD_TYPE_BOUNDED_STRING_ARRAY': 69,
        'FIELD_TYPE_BOUNDED_WSTRING_ARRAY': 70,
        'FIELD_TYPE_NESTED_TYPE_BOUNDED_SEQUENCE': 97,
        'FIELD_TYPE_INT8_BOUNDED_SEQUENCE': 98,
        'FIELD_TYPE_UINT8_BOUNDED_SEQUENCE': 99,
        'FIELD_TYPE_INT16_BOUNDED_SEQUENCE': 100,
        'FIELD_TYPE_UINT16_BOUNDED_SEQUENCE': 101,
        'FIELD_TYPE_INT32_BOUNDED_SEQUENCE': 102,
        'FIELD_TYPE_UINT32_BOUNDED_SEQUENCE': 103,
        'FIELD_TYPE_INT64_BOUNDED_SEQUENCE': 104,
        'FIELD_TYPE_UINT64_BOUNDED_SEQUENCE': 105,
        'FIELD_TYPE_FLOAT_BOUNDED_SEQUENCE': 106,
        'FIELD_TYPE_DOUBLE_BOUNDED_SEQUENCE': 107,
        'FIELD_TYPE_LONG_DOUBLE_BOUNDED_SEQUENCE': 108,
        'FIELD_TYPE_CHAR_BOUNDED_SEQUENCE': 109,
        'FIELD_TYPE_WCHAR_BOUNDED_SEQUENCE': 110,
        'FIELD_TYPE_BOOLEAN_BOUNDED_SEQUENCE': 111,
        'FIELD_TYPE_BYTE_BOUNDED_SEQUENCE': 112,
        'FIELD_TYPE_STRING_BOUNDED_SEQUENCE': 113,
        'FIELD_TYPE_WSTRING_BOUNDED_SEQUENCE': 114,
        'FIELD_TYPE_FIXED_STRING_BOUNDED_SEQUENCE': 115,
        'FIELD_TYPE_FIXED_WSTRING_BOUNDED_SEQUENCE': 116,
        'FIELD_TYPE_BOUNDED_STRING_BOUNDED_SEQUENCE': 117,
        'FIELD_TYPE_BOUNDED_WSTRING_BOUNDED_SEQUENCE': 118,
        'FIELD_TYPE_NESTED_TYPE_UNBOUNDED_SEQUENCE': 145,
        'FIELD_TYPE_INT8_UNBOUNDED_SEQUENCE': 146,
        'FIELD_TYPE_UINT8_UNBOUNDED_SEQUENCE': 147,
        'FIELD_TYPE_INT16_UNBOUNDED_SEQUENCE': 148,
        'FIELD_TYPE_UINT16_UNBOUNDED_SEQUENCE': 149,
        'FIELD_TYPE_INT32_UNBOUNDED_SEQUENCE': 150,
        'FIELD_TYPE_UINT32_UNBOUNDED_SEQUENCE': 151,
        'FIELD_TYPE_INT64_UNBOUNDED_SEQUENCE': 152,
        'FIELD_TYPE_UINT64_UNBOUNDED_SEQUENCE': 153,
        'FIELD_TYPE_FLOAT_UNBOUNDED_SEQUENCE': 154,
        'FIELD_TYPE_DOUBLE_UNBOUNDED_SEQUENCE': 155,
        'FIELD_TYPE_LONG_DOUBLE_UNBOUNDED_SEQUENCE': 156,
        'FIELD_TYPE_CHAR_UNBOUNDED_SEQUENCE': 157,
        'FIELD_TYPE_WCHAR_UNBOUNDED_SEQUENCE': 158,
        'FIELD_TYPE_BOOLEAN_UNBOUNDED_SEQUENCE': 159,
        'FIELD_TYPE_BYTE_UNBOUNDED_SEQUENCE': 160,
        'FIELD_TYPE_STRING_UNBOUNDED_SEQUENCE': 161,
        'FIELD_TYPE_WSTRING_UNBOUNDED_SEQUENCE': 162,
        'FIELD_TYPE_FIXED_STRING_UNBOUNDED_SEQUENCE': 163,
        'FIELD_TYPE_FIXED_WSTRING_UNBOUNDED_SEQUENCE': 164,
        'FIELD_TYPE_BOUNDED_STRING_UNBOUNDED_SEQUENCE': 165,
        'FIELD_TYPE_BOUNDED_WSTRING_UNBOUNDED_SEQUENCE': 166,
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
                'type_description_interfaces.msg.FieldType')
            logger.debug(
                'Failed to import needed modules for type support:\n' +
                traceback.format_exc())
        else:
            cls._CREATE_ROS_MESSAGE = module.create_ros_message_msg__msg__field_type
            cls._CONVERT_FROM_PY = module.convert_from_py_msg__msg__field_type
            cls._CONVERT_TO_PY = module.convert_to_py_msg__msg__field_type
            cls._TYPE_SUPPORT = module.type_support_msg__msg__field_type
            cls._DESTROY_ROS_MESSAGE = module.destroy_ros_message_msg__msg__field_type

    @classmethod
    def __prepare__(cls, name, bases, **kwargs):
        # list constant names here so that they appear in the help text of
        # the message class under "Data and other attributes defined here:"
        # as well as populate each message instance
        return {
            'FIELD_TYPE_NOT_SET': cls.__constants['FIELD_TYPE_NOT_SET'],
            'FIELD_TYPE_NESTED_TYPE': cls.__constants['FIELD_TYPE_NESTED_TYPE'],
            'FIELD_TYPE_INT8': cls.__constants['FIELD_TYPE_INT8'],
            'FIELD_TYPE_UINT8': cls.__constants['FIELD_TYPE_UINT8'],
            'FIELD_TYPE_INT16': cls.__constants['FIELD_TYPE_INT16'],
            'FIELD_TYPE_UINT16': cls.__constants['FIELD_TYPE_UINT16'],
            'FIELD_TYPE_INT32': cls.__constants['FIELD_TYPE_INT32'],
            'FIELD_TYPE_UINT32': cls.__constants['FIELD_TYPE_UINT32'],
            'FIELD_TYPE_INT64': cls.__constants['FIELD_TYPE_INT64'],
            'FIELD_TYPE_UINT64': cls.__constants['FIELD_TYPE_UINT64'],
            'FIELD_TYPE_FLOAT': cls.__constants['FIELD_TYPE_FLOAT'],
            'FIELD_TYPE_DOUBLE': cls.__constants['FIELD_TYPE_DOUBLE'],
            'FIELD_TYPE_LONG_DOUBLE': cls.__constants['FIELD_TYPE_LONG_DOUBLE'],
            'FIELD_TYPE_CHAR': cls.__constants['FIELD_TYPE_CHAR'],
            'FIELD_TYPE_WCHAR': cls.__constants['FIELD_TYPE_WCHAR'],
            'FIELD_TYPE_BOOLEAN': cls.__constants['FIELD_TYPE_BOOLEAN'],
            'FIELD_TYPE_BYTE': cls.__constants['FIELD_TYPE_BYTE'],
            'FIELD_TYPE_STRING': cls.__constants['FIELD_TYPE_STRING'],
            'FIELD_TYPE_WSTRING': cls.__constants['FIELD_TYPE_WSTRING'],
            'FIELD_TYPE_FIXED_STRING': cls.__constants['FIELD_TYPE_FIXED_STRING'],
            'FIELD_TYPE_FIXED_WSTRING': cls.__constants['FIELD_TYPE_FIXED_WSTRING'],
            'FIELD_TYPE_BOUNDED_STRING': cls.__constants['FIELD_TYPE_BOUNDED_STRING'],
            'FIELD_TYPE_BOUNDED_WSTRING': cls.__constants['FIELD_TYPE_BOUNDED_WSTRING'],
            'FIELD_TYPE_NESTED_TYPE_ARRAY': cls.__constants['FIELD_TYPE_NESTED_TYPE_ARRAY'],
            'FIELD_TYPE_INT8_ARRAY': cls.__constants['FIELD_TYPE_INT8_ARRAY'],
            'FIELD_TYPE_UINT8_ARRAY': cls.__constants['FIELD_TYPE_UINT8_ARRAY'],
            'FIELD_TYPE_INT16_ARRAY': cls.__constants['FIELD_TYPE_INT16_ARRAY'],
            'FIELD_TYPE_UINT16_ARRAY': cls.__constants['FIELD_TYPE_UINT16_ARRAY'],
            'FIELD_TYPE_INT32_ARRAY': cls.__constants['FIELD_TYPE_INT32_ARRAY'],
            'FIELD_TYPE_UINT32_ARRAY': cls.__constants['FIELD_TYPE_UINT32_ARRAY'],
            'FIELD_TYPE_INT64_ARRAY': cls.__constants['FIELD_TYPE_INT64_ARRAY'],
            'FIELD_TYPE_UINT64_ARRAY': cls.__constants['FIELD_TYPE_UINT64_ARRAY'],
            'FIELD_TYPE_FLOAT_ARRAY': cls.__constants['FIELD_TYPE_FLOAT_ARRAY'],
            'FIELD_TYPE_DOUBLE_ARRAY': cls.__constants['FIELD_TYPE_DOUBLE_ARRAY'],
            'FIELD_TYPE_LONG_DOUBLE_ARRAY': cls.__constants['FIELD_TYPE_LONG_DOUBLE_ARRAY'],
            'FIELD_TYPE_CHAR_ARRAY': cls.__constants['FIELD_TYPE_CHAR_ARRAY'],
            'FIELD_TYPE_WCHAR_ARRAY': cls.__constants['FIELD_TYPE_WCHAR_ARRAY'],
            'FIELD_TYPE_BOOLEAN_ARRAY': cls.__constants['FIELD_TYPE_BOOLEAN_ARRAY'],
            'FIELD_TYPE_BYTE_ARRAY': cls.__constants['FIELD_TYPE_BYTE_ARRAY'],
            'FIELD_TYPE_STRING_ARRAY': cls.__constants['FIELD_TYPE_STRING_ARRAY'],
            'FIELD_TYPE_WSTRING_ARRAY': cls.__constants['FIELD_TYPE_WSTRING_ARRAY'],
            'FIELD_TYPE_FIXED_STRING_ARRAY': cls.__constants['FIELD_TYPE_FIXED_STRING_ARRAY'],
            'FIELD_TYPE_FIXED_WSTRING_ARRAY': cls.__constants['FIELD_TYPE_FIXED_WSTRING_ARRAY'],
            'FIELD_TYPE_BOUNDED_STRING_ARRAY': cls.__constants['FIELD_TYPE_BOUNDED_STRING_ARRAY'],
            'FIELD_TYPE_BOUNDED_WSTRING_ARRAY': cls.__constants['FIELD_TYPE_BOUNDED_WSTRING_ARRAY'],
            'FIELD_TYPE_NESTED_TYPE_BOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_NESTED_TYPE_BOUNDED_SEQUENCE'],
            'FIELD_TYPE_INT8_BOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_INT8_BOUNDED_SEQUENCE'],
            'FIELD_TYPE_UINT8_BOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_UINT8_BOUNDED_SEQUENCE'],
            'FIELD_TYPE_INT16_BOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_INT16_BOUNDED_SEQUENCE'],
            'FIELD_TYPE_UINT16_BOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_UINT16_BOUNDED_SEQUENCE'],
            'FIELD_TYPE_INT32_BOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_INT32_BOUNDED_SEQUENCE'],
            'FIELD_TYPE_UINT32_BOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_UINT32_BOUNDED_SEQUENCE'],
            'FIELD_TYPE_INT64_BOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_INT64_BOUNDED_SEQUENCE'],
            'FIELD_TYPE_UINT64_BOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_UINT64_BOUNDED_SEQUENCE'],
            'FIELD_TYPE_FLOAT_BOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_FLOAT_BOUNDED_SEQUENCE'],
            'FIELD_TYPE_DOUBLE_BOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_DOUBLE_BOUNDED_SEQUENCE'],
            'FIELD_TYPE_LONG_DOUBLE_BOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_LONG_DOUBLE_BOUNDED_SEQUENCE'],
            'FIELD_TYPE_CHAR_BOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_CHAR_BOUNDED_SEQUENCE'],
            'FIELD_TYPE_WCHAR_BOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_WCHAR_BOUNDED_SEQUENCE'],
            'FIELD_TYPE_BOOLEAN_BOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_BOOLEAN_BOUNDED_SEQUENCE'],
            'FIELD_TYPE_BYTE_BOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_BYTE_BOUNDED_SEQUENCE'],
            'FIELD_TYPE_STRING_BOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_STRING_BOUNDED_SEQUENCE'],
            'FIELD_TYPE_WSTRING_BOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_WSTRING_BOUNDED_SEQUENCE'],
            'FIELD_TYPE_FIXED_STRING_BOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_FIXED_STRING_BOUNDED_SEQUENCE'],
            'FIELD_TYPE_FIXED_WSTRING_BOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_FIXED_WSTRING_BOUNDED_SEQUENCE'],
            'FIELD_TYPE_BOUNDED_STRING_BOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_BOUNDED_STRING_BOUNDED_SEQUENCE'],
            'FIELD_TYPE_BOUNDED_WSTRING_BOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_BOUNDED_WSTRING_BOUNDED_SEQUENCE'],
            'FIELD_TYPE_NESTED_TYPE_UNBOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_NESTED_TYPE_UNBOUNDED_SEQUENCE'],
            'FIELD_TYPE_INT8_UNBOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_INT8_UNBOUNDED_SEQUENCE'],
            'FIELD_TYPE_UINT8_UNBOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_UINT8_UNBOUNDED_SEQUENCE'],
            'FIELD_TYPE_INT16_UNBOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_INT16_UNBOUNDED_SEQUENCE'],
            'FIELD_TYPE_UINT16_UNBOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_UINT16_UNBOUNDED_SEQUENCE'],
            'FIELD_TYPE_INT32_UNBOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_INT32_UNBOUNDED_SEQUENCE'],
            'FIELD_TYPE_UINT32_UNBOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_UINT32_UNBOUNDED_SEQUENCE'],
            'FIELD_TYPE_INT64_UNBOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_INT64_UNBOUNDED_SEQUENCE'],
            'FIELD_TYPE_UINT64_UNBOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_UINT64_UNBOUNDED_SEQUENCE'],
            'FIELD_TYPE_FLOAT_UNBOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_FLOAT_UNBOUNDED_SEQUENCE'],
            'FIELD_TYPE_DOUBLE_UNBOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_DOUBLE_UNBOUNDED_SEQUENCE'],
            'FIELD_TYPE_LONG_DOUBLE_UNBOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_LONG_DOUBLE_UNBOUNDED_SEQUENCE'],
            'FIELD_TYPE_CHAR_UNBOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_CHAR_UNBOUNDED_SEQUENCE'],
            'FIELD_TYPE_WCHAR_UNBOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_WCHAR_UNBOUNDED_SEQUENCE'],
            'FIELD_TYPE_BOOLEAN_UNBOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_BOOLEAN_UNBOUNDED_SEQUENCE'],
            'FIELD_TYPE_BYTE_UNBOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_BYTE_UNBOUNDED_SEQUENCE'],
            'FIELD_TYPE_STRING_UNBOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_STRING_UNBOUNDED_SEQUENCE'],
            'FIELD_TYPE_WSTRING_UNBOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_WSTRING_UNBOUNDED_SEQUENCE'],
            'FIELD_TYPE_FIXED_STRING_UNBOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_FIXED_STRING_UNBOUNDED_SEQUENCE'],
            'FIELD_TYPE_FIXED_WSTRING_UNBOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_FIXED_WSTRING_UNBOUNDED_SEQUENCE'],
            'FIELD_TYPE_BOUNDED_STRING_UNBOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_BOUNDED_STRING_UNBOUNDED_SEQUENCE'],
            'FIELD_TYPE_BOUNDED_WSTRING_UNBOUNDED_SEQUENCE': cls.__constants['FIELD_TYPE_BOUNDED_WSTRING_UNBOUNDED_SEQUENCE'],
            'TYPE_ID__DEFAULT': 0,
        }

    @property
    def FIELD_TYPE_NOT_SET(self):
        """Message constant 'FIELD_TYPE_NOT_SET'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_NOT_SET']

    @property
    def FIELD_TYPE_NESTED_TYPE(self):
        """Message constant 'FIELD_TYPE_NESTED_TYPE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_NESTED_TYPE']

    @property
    def FIELD_TYPE_INT8(self):
        """Message constant 'FIELD_TYPE_INT8'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_INT8']

    @property
    def FIELD_TYPE_UINT8(self):
        """Message constant 'FIELD_TYPE_UINT8'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_UINT8']

    @property
    def FIELD_TYPE_INT16(self):
        """Message constant 'FIELD_TYPE_INT16'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_INT16']

    @property
    def FIELD_TYPE_UINT16(self):
        """Message constant 'FIELD_TYPE_UINT16'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_UINT16']

    @property
    def FIELD_TYPE_INT32(self):
        """Message constant 'FIELD_TYPE_INT32'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_INT32']

    @property
    def FIELD_TYPE_UINT32(self):
        """Message constant 'FIELD_TYPE_UINT32'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_UINT32']

    @property
    def FIELD_TYPE_INT64(self):
        """Message constant 'FIELD_TYPE_INT64'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_INT64']

    @property
    def FIELD_TYPE_UINT64(self):
        """Message constant 'FIELD_TYPE_UINT64'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_UINT64']

    @property
    def FIELD_TYPE_FLOAT(self):
        """Message constant 'FIELD_TYPE_FLOAT'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_FLOAT']

    @property
    def FIELD_TYPE_DOUBLE(self):
        """Message constant 'FIELD_TYPE_DOUBLE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_DOUBLE']

    @property
    def FIELD_TYPE_LONG_DOUBLE(self):
        """Message constant 'FIELD_TYPE_LONG_DOUBLE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_LONG_DOUBLE']

    @property
    def FIELD_TYPE_CHAR(self):
        """Message constant 'FIELD_TYPE_CHAR'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_CHAR']

    @property
    def FIELD_TYPE_WCHAR(self):
        """Message constant 'FIELD_TYPE_WCHAR'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_WCHAR']

    @property
    def FIELD_TYPE_BOOLEAN(self):
        """Message constant 'FIELD_TYPE_BOOLEAN'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_BOOLEAN']

    @property
    def FIELD_TYPE_BYTE(self):
        """Message constant 'FIELD_TYPE_BYTE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_BYTE']

    @property
    def FIELD_TYPE_STRING(self):
        """Message constant 'FIELD_TYPE_STRING'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_STRING']

    @property
    def FIELD_TYPE_WSTRING(self):
        """Message constant 'FIELD_TYPE_WSTRING'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_WSTRING']

    @property
    def FIELD_TYPE_FIXED_STRING(self):
        """Message constant 'FIELD_TYPE_FIXED_STRING'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_FIXED_STRING']

    @property
    def FIELD_TYPE_FIXED_WSTRING(self):
        """Message constant 'FIELD_TYPE_FIXED_WSTRING'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_FIXED_WSTRING']

    @property
    def FIELD_TYPE_BOUNDED_STRING(self):
        """Message constant 'FIELD_TYPE_BOUNDED_STRING'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_BOUNDED_STRING']

    @property
    def FIELD_TYPE_BOUNDED_WSTRING(self):
        """Message constant 'FIELD_TYPE_BOUNDED_WSTRING'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_BOUNDED_WSTRING']

    @property
    def FIELD_TYPE_NESTED_TYPE_ARRAY(self):
        """Message constant 'FIELD_TYPE_NESTED_TYPE_ARRAY'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_NESTED_TYPE_ARRAY']

    @property
    def FIELD_TYPE_INT8_ARRAY(self):
        """Message constant 'FIELD_TYPE_INT8_ARRAY'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_INT8_ARRAY']

    @property
    def FIELD_TYPE_UINT8_ARRAY(self):
        """Message constant 'FIELD_TYPE_UINT8_ARRAY'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_UINT8_ARRAY']

    @property
    def FIELD_TYPE_INT16_ARRAY(self):
        """Message constant 'FIELD_TYPE_INT16_ARRAY'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_INT16_ARRAY']

    @property
    def FIELD_TYPE_UINT16_ARRAY(self):
        """Message constant 'FIELD_TYPE_UINT16_ARRAY'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_UINT16_ARRAY']

    @property
    def FIELD_TYPE_INT32_ARRAY(self):
        """Message constant 'FIELD_TYPE_INT32_ARRAY'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_INT32_ARRAY']

    @property
    def FIELD_TYPE_UINT32_ARRAY(self):
        """Message constant 'FIELD_TYPE_UINT32_ARRAY'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_UINT32_ARRAY']

    @property
    def FIELD_TYPE_INT64_ARRAY(self):
        """Message constant 'FIELD_TYPE_INT64_ARRAY'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_INT64_ARRAY']

    @property
    def FIELD_TYPE_UINT64_ARRAY(self):
        """Message constant 'FIELD_TYPE_UINT64_ARRAY'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_UINT64_ARRAY']

    @property
    def FIELD_TYPE_FLOAT_ARRAY(self):
        """Message constant 'FIELD_TYPE_FLOAT_ARRAY'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_FLOAT_ARRAY']

    @property
    def FIELD_TYPE_DOUBLE_ARRAY(self):
        """Message constant 'FIELD_TYPE_DOUBLE_ARRAY'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_DOUBLE_ARRAY']

    @property
    def FIELD_TYPE_LONG_DOUBLE_ARRAY(self):
        """Message constant 'FIELD_TYPE_LONG_DOUBLE_ARRAY'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_LONG_DOUBLE_ARRAY']

    @property
    def FIELD_TYPE_CHAR_ARRAY(self):
        """Message constant 'FIELD_TYPE_CHAR_ARRAY'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_CHAR_ARRAY']

    @property
    def FIELD_TYPE_WCHAR_ARRAY(self):
        """Message constant 'FIELD_TYPE_WCHAR_ARRAY'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_WCHAR_ARRAY']

    @property
    def FIELD_TYPE_BOOLEAN_ARRAY(self):
        """Message constant 'FIELD_TYPE_BOOLEAN_ARRAY'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_BOOLEAN_ARRAY']

    @property
    def FIELD_TYPE_BYTE_ARRAY(self):
        """Message constant 'FIELD_TYPE_BYTE_ARRAY'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_BYTE_ARRAY']

    @property
    def FIELD_TYPE_STRING_ARRAY(self):
        """Message constant 'FIELD_TYPE_STRING_ARRAY'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_STRING_ARRAY']

    @property
    def FIELD_TYPE_WSTRING_ARRAY(self):
        """Message constant 'FIELD_TYPE_WSTRING_ARRAY'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_WSTRING_ARRAY']

    @property
    def FIELD_TYPE_FIXED_STRING_ARRAY(self):
        """Message constant 'FIELD_TYPE_FIXED_STRING_ARRAY'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_FIXED_STRING_ARRAY']

    @property
    def FIELD_TYPE_FIXED_WSTRING_ARRAY(self):
        """Message constant 'FIELD_TYPE_FIXED_WSTRING_ARRAY'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_FIXED_WSTRING_ARRAY']

    @property
    def FIELD_TYPE_BOUNDED_STRING_ARRAY(self):
        """Message constant 'FIELD_TYPE_BOUNDED_STRING_ARRAY'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_BOUNDED_STRING_ARRAY']

    @property
    def FIELD_TYPE_BOUNDED_WSTRING_ARRAY(self):
        """Message constant 'FIELD_TYPE_BOUNDED_WSTRING_ARRAY'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_BOUNDED_WSTRING_ARRAY']

    @property
    def FIELD_TYPE_NESTED_TYPE_BOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_NESTED_TYPE_BOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_NESTED_TYPE_BOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_INT8_BOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_INT8_BOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_INT8_BOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_UINT8_BOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_UINT8_BOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_UINT8_BOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_INT16_BOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_INT16_BOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_INT16_BOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_UINT16_BOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_UINT16_BOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_UINT16_BOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_INT32_BOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_INT32_BOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_INT32_BOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_UINT32_BOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_UINT32_BOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_UINT32_BOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_INT64_BOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_INT64_BOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_INT64_BOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_UINT64_BOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_UINT64_BOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_UINT64_BOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_FLOAT_BOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_FLOAT_BOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_FLOAT_BOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_DOUBLE_BOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_DOUBLE_BOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_DOUBLE_BOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_LONG_DOUBLE_BOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_LONG_DOUBLE_BOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_LONG_DOUBLE_BOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_CHAR_BOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_CHAR_BOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_CHAR_BOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_WCHAR_BOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_WCHAR_BOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_WCHAR_BOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_BOOLEAN_BOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_BOOLEAN_BOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_BOOLEAN_BOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_BYTE_BOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_BYTE_BOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_BYTE_BOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_STRING_BOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_STRING_BOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_STRING_BOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_WSTRING_BOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_WSTRING_BOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_WSTRING_BOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_FIXED_STRING_BOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_FIXED_STRING_BOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_FIXED_STRING_BOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_FIXED_WSTRING_BOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_FIXED_WSTRING_BOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_FIXED_WSTRING_BOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_BOUNDED_STRING_BOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_BOUNDED_STRING_BOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_BOUNDED_STRING_BOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_BOUNDED_WSTRING_BOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_BOUNDED_WSTRING_BOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_BOUNDED_WSTRING_BOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_NESTED_TYPE_UNBOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_NESTED_TYPE_UNBOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_NESTED_TYPE_UNBOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_INT8_UNBOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_INT8_UNBOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_INT8_UNBOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_UINT8_UNBOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_UINT8_UNBOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_UINT8_UNBOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_INT16_UNBOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_INT16_UNBOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_INT16_UNBOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_UINT16_UNBOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_UINT16_UNBOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_UINT16_UNBOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_INT32_UNBOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_INT32_UNBOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_INT32_UNBOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_UINT32_UNBOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_UINT32_UNBOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_UINT32_UNBOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_INT64_UNBOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_INT64_UNBOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_INT64_UNBOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_UINT64_UNBOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_UINT64_UNBOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_UINT64_UNBOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_FLOAT_UNBOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_FLOAT_UNBOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_FLOAT_UNBOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_DOUBLE_UNBOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_DOUBLE_UNBOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_DOUBLE_UNBOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_LONG_DOUBLE_UNBOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_LONG_DOUBLE_UNBOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_LONG_DOUBLE_UNBOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_CHAR_UNBOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_CHAR_UNBOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_CHAR_UNBOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_WCHAR_UNBOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_WCHAR_UNBOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_WCHAR_UNBOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_BOOLEAN_UNBOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_BOOLEAN_UNBOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_BOOLEAN_UNBOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_BYTE_UNBOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_BYTE_UNBOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_BYTE_UNBOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_STRING_UNBOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_STRING_UNBOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_STRING_UNBOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_WSTRING_UNBOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_WSTRING_UNBOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_WSTRING_UNBOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_FIXED_STRING_UNBOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_FIXED_STRING_UNBOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_FIXED_STRING_UNBOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_FIXED_WSTRING_UNBOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_FIXED_WSTRING_UNBOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_FIXED_WSTRING_UNBOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_BOUNDED_STRING_UNBOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_BOUNDED_STRING_UNBOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_BOUNDED_STRING_UNBOUNDED_SEQUENCE']

    @property
    def FIELD_TYPE_BOUNDED_WSTRING_UNBOUNDED_SEQUENCE(self):
        """Message constant 'FIELD_TYPE_BOUNDED_WSTRING_UNBOUNDED_SEQUENCE'."""
        return Metaclass_FieldType.__constants['FIELD_TYPE_BOUNDED_WSTRING_UNBOUNDED_SEQUENCE']

    @property
    def TYPE_ID__DEFAULT(cls):
        """Return default value for message field 'type_id'."""
        return 0


class FieldType(metaclass=Metaclass_FieldType):
    """
    Message class 'FieldType'.

    Constants:
      FIELD_TYPE_NOT_SET
      FIELD_TYPE_NESTED_TYPE
      FIELD_TYPE_INT8
      FIELD_TYPE_UINT8
      FIELD_TYPE_INT16
      FIELD_TYPE_UINT16
      FIELD_TYPE_INT32
      FIELD_TYPE_UINT32
      FIELD_TYPE_INT64
      FIELD_TYPE_UINT64
      FIELD_TYPE_FLOAT
      FIELD_TYPE_DOUBLE
      FIELD_TYPE_LONG_DOUBLE
      FIELD_TYPE_CHAR
      FIELD_TYPE_WCHAR
      FIELD_TYPE_BOOLEAN
      FIELD_TYPE_BYTE
      FIELD_TYPE_STRING
      FIELD_TYPE_WSTRING
      FIELD_TYPE_FIXED_STRING
      FIELD_TYPE_FIXED_WSTRING
      FIELD_TYPE_BOUNDED_STRING
      FIELD_TYPE_BOUNDED_WSTRING
      FIELD_TYPE_NESTED_TYPE_ARRAY
      FIELD_TYPE_INT8_ARRAY
      FIELD_TYPE_UINT8_ARRAY
      FIELD_TYPE_INT16_ARRAY
      FIELD_TYPE_UINT16_ARRAY
      FIELD_TYPE_INT32_ARRAY
      FIELD_TYPE_UINT32_ARRAY
      FIELD_TYPE_INT64_ARRAY
      FIELD_TYPE_UINT64_ARRAY
      FIELD_TYPE_FLOAT_ARRAY
      FIELD_TYPE_DOUBLE_ARRAY
      FIELD_TYPE_LONG_DOUBLE_ARRAY
      FIELD_TYPE_CHAR_ARRAY
      FIELD_TYPE_WCHAR_ARRAY
      FIELD_TYPE_BOOLEAN_ARRAY
      FIELD_TYPE_BYTE_ARRAY
      FIELD_TYPE_STRING_ARRAY
      FIELD_TYPE_WSTRING_ARRAY
      FIELD_TYPE_FIXED_STRING_ARRAY
      FIELD_TYPE_FIXED_WSTRING_ARRAY
      FIELD_TYPE_BOUNDED_STRING_ARRAY
      FIELD_TYPE_BOUNDED_WSTRING_ARRAY
      FIELD_TYPE_NESTED_TYPE_BOUNDED_SEQUENCE
      FIELD_TYPE_INT8_BOUNDED_SEQUENCE
      FIELD_TYPE_UINT8_BOUNDED_SEQUENCE
      FIELD_TYPE_INT16_BOUNDED_SEQUENCE
      FIELD_TYPE_UINT16_BOUNDED_SEQUENCE
      FIELD_TYPE_INT32_BOUNDED_SEQUENCE
      FIELD_TYPE_UINT32_BOUNDED_SEQUENCE
      FIELD_TYPE_INT64_BOUNDED_SEQUENCE
      FIELD_TYPE_UINT64_BOUNDED_SEQUENCE
      FIELD_TYPE_FLOAT_BOUNDED_SEQUENCE
      FIELD_TYPE_DOUBLE_BOUNDED_SEQUENCE
      FIELD_TYPE_LONG_DOUBLE_BOUNDED_SEQUENCE
      FIELD_TYPE_CHAR_BOUNDED_SEQUENCE
      FIELD_TYPE_WCHAR_BOUNDED_SEQUENCE
      FIELD_TYPE_BOOLEAN_BOUNDED_SEQUENCE
      FIELD_TYPE_BYTE_BOUNDED_SEQUENCE
      FIELD_TYPE_STRING_BOUNDED_SEQUENCE
      FIELD_TYPE_WSTRING_BOUNDED_SEQUENCE
      FIELD_TYPE_FIXED_STRING_BOUNDED_SEQUENCE
      FIELD_TYPE_FIXED_WSTRING_BOUNDED_SEQUENCE
      FIELD_TYPE_BOUNDED_STRING_BOUNDED_SEQUENCE
      FIELD_TYPE_BOUNDED_WSTRING_BOUNDED_SEQUENCE
      FIELD_TYPE_NESTED_TYPE_UNBOUNDED_SEQUENCE
      FIELD_TYPE_INT8_UNBOUNDED_SEQUENCE
      FIELD_TYPE_UINT8_UNBOUNDED_SEQUENCE
      FIELD_TYPE_INT16_UNBOUNDED_SEQUENCE
      FIELD_TYPE_UINT16_UNBOUNDED_SEQUENCE
      FIELD_TYPE_INT32_UNBOUNDED_SEQUENCE
      FIELD_TYPE_UINT32_UNBOUNDED_SEQUENCE
      FIELD_TYPE_INT64_UNBOUNDED_SEQUENCE
      FIELD_TYPE_UINT64_UNBOUNDED_SEQUENCE
      FIELD_TYPE_FLOAT_UNBOUNDED_SEQUENCE
      FIELD_TYPE_DOUBLE_UNBOUNDED_SEQUENCE
      FIELD_TYPE_LONG_DOUBLE_UNBOUNDED_SEQUENCE
      FIELD_TYPE_CHAR_UNBOUNDED_SEQUENCE
      FIELD_TYPE_WCHAR_UNBOUNDED_SEQUENCE
      FIELD_TYPE_BOOLEAN_UNBOUNDED_SEQUENCE
      FIELD_TYPE_BYTE_UNBOUNDED_SEQUENCE
      FIELD_TYPE_STRING_UNBOUNDED_SEQUENCE
      FIELD_TYPE_WSTRING_UNBOUNDED_SEQUENCE
      FIELD_TYPE_FIXED_STRING_UNBOUNDED_SEQUENCE
      FIELD_TYPE_FIXED_WSTRING_UNBOUNDED_SEQUENCE
      FIELD_TYPE_BOUNDED_STRING_UNBOUNDED_SEQUENCE
      FIELD_TYPE_BOUNDED_WSTRING_UNBOUNDED_SEQUENCE
    """

    __slots__ = [
        '_type_id',
        '_capacity',
        '_string_capacity',
        '_nested_type_name',
        '_check_fields',
    ]

    _fields_and_field_types = {
        'type_id': 'uint8',
        'capacity': 'uint64',
        'string_capacity': 'uint64',
        'nested_type_name': 'string<255>',
    }

    # This attribute is used to store an rosidl_parser.definition variable
    # related to the data type of each of the components the message.
    SLOT_TYPES = (
        rosidl_parser.definition.BasicType('uint8'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint64'),  # noqa: E501
        rosidl_parser.definition.BasicType('uint64'),  # noqa: E501
        rosidl_parser.definition.BoundedString(255),  # noqa: E501
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
        self.type_id = kwargs.get(
            'type_id', FieldType.TYPE_ID__DEFAULT)
        self.capacity = kwargs.get('capacity', int())
        self.string_capacity = kwargs.get('string_capacity', int())
        self.nested_type_name = kwargs.get('nested_type_name', str())

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
        if self.type_id != other.type_id:
            return False
        if self.capacity != other.capacity:
            return False
        if self.string_capacity != other.string_capacity:
            return False
        if self.nested_type_name != other.nested_type_name:
            return False
        return True

    @classmethod
    def get_fields_and_field_types(cls):
        from copy import copy
        return copy(cls._fields_and_field_types)

    @builtins.property
    def type_id(self):
        """Message field 'type_id'."""
        return self._type_id

    @type_id.setter
    def type_id(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'type_id' field must be of type 'int'"
            assert value >= 0 and value < 256, \
                "The 'type_id' field must be an unsigned integer in [0, 255]"
        self._type_id = value

    @builtins.property
    def capacity(self):
        """Message field 'capacity'."""
        return self._capacity

    @capacity.setter
    def capacity(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'capacity' field must be of type 'int'"
            assert value >= 0 and value < 18446744073709551616, \
                "The 'capacity' field must be an unsigned integer in [0, 18446744073709551615]"
        self._capacity = value

    @builtins.property
    def string_capacity(self):
        """Message field 'string_capacity'."""
        return self._string_capacity

    @string_capacity.setter
    def string_capacity(self, value):
        if self._check_fields:
            assert \
                isinstance(value, int), \
                "The 'string_capacity' field must be of type 'int'"
            assert value >= 0 and value < 18446744073709551616, \
                "The 'string_capacity' field must be an unsigned integer in [0, 18446744073709551615]"
        self._string_capacity = value

    @builtins.property
    def nested_type_name(self):
        """Message field 'nested_type_name'."""
        return self._nested_type_name

    @nested_type_name.setter
    def nested_type_name(self, value):
        if self._check_fields:
            from collections import UserString
            assert \
                (isinstance(value, (str, UserString)) and
                 len(value) <= 255), \
                "The 'nested_type_name' field must be string value " \
                'not longer than 255'
        self._nested_type_name = value
