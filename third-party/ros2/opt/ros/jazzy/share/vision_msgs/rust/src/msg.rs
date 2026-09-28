#[cfg(feature = "serde")]
use serde::{Deserialize, Serialize};



// Corresponds to vision_msgs__msg__BoundingBox2D
/// A 2D bounding box that can be rotated about its center.
/// All dimensions are in pixels, but represented using floating-point
///   values to allow sub-pixel precision. If an exact pixel crop is required
///   for a rotated bounding box, it can be calculated using Bresenham's line
///   algorithm.

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct BoundingBox2D {
    /// The 2D position (in pixels) and orientation of the bounding box center.
    pub center: super::msg::Pose2D,

    /// The total size (in pixels) of the bounding box surrounding the object relative
    ///   to the pose of its center.
    pub size_x: f64,


    // This member is not documented.
    #[allow(missing_docs)]
    pub size_y: f64,

}



impl Default for BoundingBox2D {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::BoundingBox2D::default())
  }
}

impl rosidl_runtime_rs::Message for BoundingBox2D {
  type RmwMsg = super::msg::rmw::BoundingBox2D;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        center: super::msg::Pose2D::into_rmw_message(std::borrow::Cow::Owned(msg.center)).into_owned(),
        size_x: msg.size_x,
        size_y: msg.size_y,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        center: super::msg::Pose2D::into_rmw_message(std::borrow::Cow::Borrowed(&msg.center)).into_owned(),
      size_x: msg.size_x,
      size_y: msg.size_y,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      center: super::msg::Pose2D::from_rmw_message(msg.center),
      size_x: msg.size_x,
      size_y: msg.size_y,
    }
  }
}


// Corresponds to vision_msgs__msg__BoundingBox2DArray

// This struct is not documented.
#[allow(missing_docs)]

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct BoundingBox2DArray {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::Header,


    // This member is not documented.
    #[allow(missing_docs)]
    pub boxes: Vec<super::msg::BoundingBox2D>,

}



impl Default for BoundingBox2DArray {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::BoundingBox2DArray::default())
  }
}

impl rosidl_runtime_rs::Message for BoundingBox2DArray {
  type RmwMsg = super::msg::rmw::BoundingBox2DArray;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        boxes: msg.boxes
          .into_iter()
          .map(|elem| super::msg::BoundingBox2D::into_rmw_message(std::borrow::Cow::Owned(elem)).into_owned())
          .collect(),
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
        boxes: msg.boxes
          .iter()
          .map(|elem| super::msg::BoundingBox2D::into_rmw_message(std::borrow::Cow::Borrowed(elem)).into_owned())
          .collect(),
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      boxes: msg.boxes
          .into_iter()
          .map(super::msg::BoundingBox2D::from_rmw_message)
          .collect(),
    }
  }
}


// Corresponds to vision_msgs__msg__BoundingBox3D
/// A 3D bounding box that can be positioned and rotated about its center (6 DOF)
/// Dimensions of this box are in meters, and as such, it may be migrated to
///   another package, such as geometry_msgs, in the future.

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct BoundingBox3D {
    /// The 3D position and orientation of the bounding box center
    pub center: geometry_msgs::msg::Pose,

    /// The total size of the bounding box, in meters, surrounding the object's center
    ///   pose.
    pub size: geometry_msgs::msg::Vector3,

}



impl Default for BoundingBox3D {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::BoundingBox3D::default())
  }
}

impl rosidl_runtime_rs::Message for BoundingBox3D {
  type RmwMsg = super::msg::rmw::BoundingBox3D;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        center: geometry_msgs::msg::Pose::into_rmw_message(std::borrow::Cow::Owned(msg.center)).into_owned(),
        size: geometry_msgs::msg::Vector3::into_rmw_message(std::borrow::Cow::Owned(msg.size)).into_owned(),
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        center: geometry_msgs::msg::Pose::into_rmw_message(std::borrow::Cow::Borrowed(&msg.center)).into_owned(),
        size: geometry_msgs::msg::Vector3::into_rmw_message(std::borrow::Cow::Borrowed(&msg.size)).into_owned(),
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      center: geometry_msgs::msg::Pose::from_rmw_message(msg.center),
      size: geometry_msgs::msg::Vector3::from_rmw_message(msg.size),
    }
  }
}


// Corresponds to vision_msgs__msg__BoundingBox3DArray

// This struct is not documented.
#[allow(missing_docs)]

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct BoundingBox3DArray {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::Header,


    // This member is not documented.
    #[allow(missing_docs)]
    pub boxes: Vec<super::msg::BoundingBox3D>,

}



impl Default for BoundingBox3DArray {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::BoundingBox3DArray::default())
  }
}

impl rosidl_runtime_rs::Message for BoundingBox3DArray {
  type RmwMsg = super::msg::rmw::BoundingBox3DArray;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        boxes: msg.boxes
          .into_iter()
          .map(|elem| super::msg::BoundingBox3D::into_rmw_message(std::borrow::Cow::Owned(elem)).into_owned())
          .collect(),
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
        boxes: msg.boxes
          .iter()
          .map(|elem| super::msg::BoundingBox3D::into_rmw_message(std::borrow::Cow::Borrowed(elem)).into_owned())
          .collect(),
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      boxes: msg.boxes
          .into_iter()
          .map(super::msg::BoundingBox3D::from_rmw_message)
          .collect(),
    }
  }
}


// Corresponds to vision_msgs__msg__Classification
/// Defines a classification result.
///
/// This result does not contain any position information. It is designed for
///   classifiers, which simply provide class probabilities given an instance of
///   source data (e.g., an image or a point cloud).

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct Classification {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::Header,

    /// A list of class probabilities. This list need not provide a probability for
    ///   every possible class, just ones that are nonzero, or above some
    ///   user-defined threshold.
    pub results: Vec<super::msg::ObjectHypothesis>,

}



impl Default for Classification {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::Classification::default())
  }
}

impl rosidl_runtime_rs::Message for Classification {
  type RmwMsg = super::msg::rmw::Classification;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        results: msg.results
          .into_iter()
          .map(|elem| super::msg::ObjectHypothesis::into_rmw_message(std::borrow::Cow::Owned(elem)).into_owned())
          .collect(),
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
        results: msg.results
          .iter()
          .map(|elem| super::msg::ObjectHypothesis::into_rmw_message(std::borrow::Cow::Borrowed(elem)).into_owned())
          .collect(),
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      results: msg.results
          .into_iter()
          .map(super::msg::ObjectHypothesis::from_rmw_message)
          .collect(),
    }
  }
}


// Corresponds to vision_msgs__msg__Detection2DArray
/// A list of 2D detections, for a multi-object 2D detector.

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct Detection2DArray {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::Header,

    /// A list of the detected proposals. A multi-proposal detector might generate
    ///   this list with many candidate detections generated from a single input.
    pub detections: Vec<super::msg::Detection2D>,

}



impl Default for Detection2DArray {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::Detection2DArray::default())
  }
}

impl rosidl_runtime_rs::Message for Detection2DArray {
  type RmwMsg = super::msg::rmw::Detection2DArray;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        detections: msg.detections
          .into_iter()
          .map(|elem| super::msg::Detection2D::into_rmw_message(std::borrow::Cow::Owned(elem)).into_owned())
          .collect(),
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
        detections: msg.detections
          .iter()
          .map(|elem| super::msg::Detection2D::into_rmw_message(std::borrow::Cow::Borrowed(elem)).into_owned())
          .collect(),
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      detections: msg.detections
          .into_iter()
          .map(super::msg::Detection2D::from_rmw_message)
          .collect(),
    }
  }
}


// Corresponds to vision_msgs__msg__Detection2D
/// Defines a 2D detection result.
///
/// This is similar to a 2D classification, but includes position information,
///   allowing a classification result for a specific crop or image point to
///   to be located in the larger image.

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct Detection2D {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::Header,

    /// Class probabilities
    pub results: Vec<super::msg::ObjectHypothesisWithPose>,

    /// 2D bounding box surrounding the object.
    pub bbox: super::msg::BoundingBox2D,

    /// ID used for consistency across multiple detection messages. Detections
    /// of the same object in different detection messages should have the same id.
    /// This field may be empty.
    pub id: std::string::String,

}



impl Default for Detection2D {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::Detection2D::default())
  }
}

impl rosidl_runtime_rs::Message for Detection2D {
  type RmwMsg = super::msg::rmw::Detection2D;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        results: msg.results
          .into_iter()
          .map(|elem| super::msg::ObjectHypothesisWithPose::into_rmw_message(std::borrow::Cow::Owned(elem)).into_owned())
          .collect(),
        bbox: super::msg::BoundingBox2D::into_rmw_message(std::borrow::Cow::Owned(msg.bbox)).into_owned(),
        id: msg.id.as_str().into(),
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
        results: msg.results
          .iter()
          .map(|elem| super::msg::ObjectHypothesisWithPose::into_rmw_message(std::borrow::Cow::Borrowed(elem)).into_owned())
          .collect(),
        bbox: super::msg::BoundingBox2D::into_rmw_message(std::borrow::Cow::Borrowed(&msg.bbox)).into_owned(),
        id: msg.id.as_str().into(),
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      results: msg.results
          .into_iter()
          .map(super::msg::ObjectHypothesisWithPose::from_rmw_message)
          .collect(),
      bbox: super::msg::BoundingBox2D::from_rmw_message(msg.bbox),
      id: msg.id.to_string(),
    }
  }
}


// Corresponds to vision_msgs__msg__Detection3DArray
/// A list of 3D detections, for a multi-object 3D detector.

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct Detection3DArray {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::Header,

    /// A list of the detected proposals. A multi-proposal detector might generate
    ///   this list with many candidate detections generated from a single input.
    pub detections: Vec<super::msg::Detection3D>,

}



impl Default for Detection3DArray {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::Detection3DArray::default())
  }
}

impl rosidl_runtime_rs::Message for Detection3DArray {
  type RmwMsg = super::msg::rmw::Detection3DArray;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        detections: msg.detections
          .into_iter()
          .map(|elem| super::msg::Detection3D::into_rmw_message(std::borrow::Cow::Owned(elem)).into_owned())
          .collect(),
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
        detections: msg.detections
          .iter()
          .map(|elem| super::msg::Detection3D::into_rmw_message(std::borrow::Cow::Borrowed(elem)).into_owned())
          .collect(),
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      detections: msg.detections
          .into_iter()
          .map(super::msg::Detection3D::from_rmw_message)
          .collect(),
    }
  }
}


// Corresponds to vision_msgs__msg__Detection3D
/// Defines a 3D detection result.
///
/// This extends a basic 3D classification by including the pose of the
/// detected object.

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct Detection3D {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::Header,

    /// Class probabilities. Does not have to include hypotheses for all possible
    ///   object ids, the scores for any ids not listed are assumed to be 0.
    pub results: Vec<super::msg::ObjectHypothesisWithPose>,

    /// 3D bounding box surrounding the object.
    pub bbox: super::msg::BoundingBox3D,

    /// ID used for consistency across multiple detection messages. Detections
    /// of the same object in different detection messages should have the same id.
    /// This field may be empty.
    pub id: std::string::String,

}



impl Default for Detection3D {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::Detection3D::default())
  }
}

impl rosidl_runtime_rs::Message for Detection3D {
  type RmwMsg = super::msg::rmw::Detection3D;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        results: msg.results
          .into_iter()
          .map(|elem| super::msg::ObjectHypothesisWithPose::into_rmw_message(std::borrow::Cow::Owned(elem)).into_owned())
          .collect(),
        bbox: super::msg::BoundingBox3D::into_rmw_message(std::borrow::Cow::Owned(msg.bbox)).into_owned(),
        id: msg.id.as_str().into(),
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
        results: msg.results
          .iter()
          .map(|elem| super::msg::ObjectHypothesisWithPose::into_rmw_message(std::borrow::Cow::Borrowed(elem)).into_owned())
          .collect(),
        bbox: super::msg::BoundingBox3D::into_rmw_message(std::borrow::Cow::Borrowed(&msg.bbox)).into_owned(),
        id: msg.id.as_str().into(),
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      results: msg.results
          .into_iter()
          .map(super::msg::ObjectHypothesisWithPose::from_rmw_message)
          .collect(),
      bbox: super::msg::BoundingBox3D::from_rmw_message(msg.bbox),
      id: msg.id.to_string(),
    }
  }
}


// Corresponds to vision_msgs__msg__LabelInfo
/// Provides meta-information about a visual pipeline.
///
/// This message serves a similar purpose to sensor_msgs/CameraInfo, but instead
///   of being tied to hardware, it represents information about a specific
///   computer vision pipeline. This information stays constant (or relatively
///   constant) over time, and so it is wasteful to send it with each individual
///   result. By listening to these messages, subscribers will receive
///   the context in which published vision messages are to be interpreted.
/// Each vision pipeline should publish its LabelInfo messages to its own topic,
///   in a manner similar to CameraInfo.
/// This message is meant to allow converting data from vision pipelines that
///   return id based classifications back to human readable string class names.

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct LabelInfo {
    /// Used for sequencing
    pub header: std_msgs::msg::Header,

    /// An array of uint16 keys and string values containing the association
    ///   between class identifiers and their names. According to the amount
    ///   of classes and the datatype used to store their ids internally, the
    ///   maxiumum class id allowed (65535 for uint16 and 255 for uint8) belongs to
    ///   the "UNLABELED" class.
    pub class_map: Vec<super::msg::VisionClass>,

    /// The value between 0-1 used as confidence threshold for the inference.
    pub threshold: f32,

}



impl Default for LabelInfo {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::LabelInfo::default())
  }
}

impl rosidl_runtime_rs::Message for LabelInfo {
  type RmwMsg = super::msg::rmw::LabelInfo;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        class_map: msg.class_map
          .into_iter()
          .map(|elem| super::msg::VisionClass::into_rmw_message(std::borrow::Cow::Owned(elem)).into_owned())
          .collect(),
        threshold: msg.threshold,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
        class_map: msg.class_map
          .iter()
          .map(|elem| super::msg::VisionClass::into_rmw_message(std::borrow::Cow::Borrowed(elem)).into_owned())
          .collect(),
      threshold: msg.threshold,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      class_map: msg.class_map
          .into_iter()
          .map(super::msg::VisionClass::from_rmw_message)
          .collect(),
      threshold: msg.threshold,
    }
  }
}


// Corresponds to vision_msgs__msg__ObjectHypothesis
/// An object hypothesis that contains no pose information.
/// If you would like to define an array of ObjectHypothesis messages,
///   please see the Classification message type.

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct ObjectHypothesis {
    /// The unique ID of the object class. To get additional information about
    ///   this ID, such as its human-readable class name, listeners should perform a
    ///   lookup in a metadata database. See vision_msgs/VisionInfo.msg for more detail.
    pub class_id: std::string::String,

    /// The probability or confidence value of the detected object. By convention,
    ///   this value should lie in the range.
    pub score: f64,

}



impl Default for ObjectHypothesis {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::ObjectHypothesis::default())
  }
}

impl rosidl_runtime_rs::Message for ObjectHypothesis {
  type RmwMsg = super::msg::rmw::ObjectHypothesis;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        class_id: msg.class_id.as_str().into(),
        score: msg.score,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        class_id: msg.class_id.as_str().into(),
      score: msg.score,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      class_id: msg.class_id.to_string(),
      score: msg.score,
    }
  }
}


// Corresponds to vision_msgs__msg__ObjectHypothesisWithPose
/// An object hypothesis that contains pose information.
/// If you would like to define an array of ObjectHypothesisWithPose messages,
///   please see the Detection2D or Detection3D message types.

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct ObjectHypothesisWithPose {
    /// The object hypothesis (ID and score).
    pub hypothesis: super::msg::ObjectHypothesis,

    /// The 6D pose of the object hypothesis. This pose should be
    ///   defined as the pose of some fixed reference point on the object, such as
    ///   the geometric center of the bounding box, the center of mass of the
    ///   object or the origin of a reference mesh of the object.
    /// Note that this pose is not stamped; frame information can be defined by
    ///   parent messages.
    /// Also note that different classes predicted for the same input data may have
    ///   different predicted 6D poses.
    pub pose: geometry_msgs::msg::PoseWithCovariance,

}



impl Default for ObjectHypothesisWithPose {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::ObjectHypothesisWithPose::default())
  }
}

impl rosidl_runtime_rs::Message for ObjectHypothesisWithPose {
  type RmwMsg = super::msg::rmw::ObjectHypothesisWithPose;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        hypothesis: super::msg::ObjectHypothesis::into_rmw_message(std::borrow::Cow::Owned(msg.hypothesis)).into_owned(),
        pose: geometry_msgs::msg::PoseWithCovariance::into_rmw_message(std::borrow::Cow::Owned(msg.pose)).into_owned(),
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        hypothesis: super::msg::ObjectHypothesis::into_rmw_message(std::borrow::Cow::Borrowed(&msg.hypothesis)).into_owned(),
        pose: geometry_msgs::msg::PoseWithCovariance::into_rmw_message(std::borrow::Cow::Borrowed(&msg.pose)).into_owned(),
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      hypothesis: super::msg::ObjectHypothesis::from_rmw_message(msg.hypothesis),
      pose: geometry_msgs::msg::PoseWithCovariance::from_rmw_message(msg.pose),
    }
  }
}


// Corresponds to vision_msgs__msg__VisionClass
/// A key value pair that maps an integer class_id to a string class label
///   in computer vision systems.

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct VisionClass {
    /// The int value that identifies the class.
    /// Elements identified with 65535, the maximum uint16 value are assumed
    ///   to belong to the "UNLABELED" class. For vision pipelines using less
    ///   than 255 classes the "UNLABELED" is the maximum value in the uint8
    ///   range.
    pub class_id: u16,

    /// The name of the class represented by the class_id
    pub class_name: std::string::String,

}



impl Default for VisionClass {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::VisionClass::default())
  }
}

impl rosidl_runtime_rs::Message for VisionClass {
  type RmwMsg = super::msg::rmw::VisionClass;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        class_id: msg.class_id,
        class_name: msg.class_name.as_str().into(),
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
      class_id: msg.class_id,
        class_name: msg.class_name.as_str().into(),
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      class_id: msg.class_id,
      class_name: msg.class_name.to_string(),
    }
  }
}


// Corresponds to vision_msgs__msg__Point2D
/// Represents a 2D point in pixel coordinates.
/// XY matches the sensor_msgs/Image convention: X is positive right and Y is positive down.

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct Point2D {

    // This member is not documented.
    #[allow(missing_docs)]
    pub x: f64,


    // This member is not documented.
    #[allow(missing_docs)]
    pub y: f64,

}



impl Default for Point2D {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::Point2D::default())
  }
}

impl rosidl_runtime_rs::Message for Point2D {
  type RmwMsg = super::msg::rmw::Point2D;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        x: msg.x,
        y: msg.y,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
      x: msg.x,
      y: msg.y,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      x: msg.x,
      y: msg.y,
    }
  }
}


// Corresponds to vision_msgs__msg__Pose2D
/// Represents a 2D pose (coordinates and a radian rotation). Rotation is positive counterclockwise.

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct Pose2D {

    // This member is not documented.
    #[allow(missing_docs)]
    pub position: super::msg::Point2D,


    // This member is not documented.
    #[allow(missing_docs)]
    pub theta: f64,

}



impl Default for Pose2D {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::Pose2D::default())
  }
}

impl rosidl_runtime_rs::Message for Pose2D {
  type RmwMsg = super::msg::rmw::Pose2D;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        position: super::msg::Point2D::into_rmw_message(std::borrow::Cow::Owned(msg.position)).into_owned(),
        theta: msg.theta,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        position: super::msg::Point2D::into_rmw_message(std::borrow::Cow::Borrowed(&msg.position)).into_owned(),
      theta: msg.theta,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      position: super::msg::Point2D::from_rmw_message(msg.position),
      theta: msg.theta,
    }
  }
}


// Corresponds to vision_msgs__msg__VisionInfo
/// Provides meta-information about a visual pipeline.
///
/// This message serves a similar purpose to sensor_msgs/CameraInfo, but instead
///   of being tied to hardware, it represents information about a specific
///   computer vision pipeline. This information stays constant (or relatively
///   constant) over time, and so it is wasteful to send it with each individual
///   result. By listening to these messages, subscribers will receive
///   the context in which published vision messages are to be interpreted.
/// Each vision pipeline should publish its VisionInfo messages to its own topic,
///   in a manner similar to CameraInfo.

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct VisionInfo {
    /// Used for sequencing
    pub header: std_msgs::msg::Header,

    /// Name of the vision pipeline. This should be a value that is meaningful to an
    ///   outside user.
    pub method: std::string::String,

    /// Location where the metadata database is stored. The recommended location is
    ///   as an XML string on the ROS parameter server, but the exact implementation
    ///   and information is left up to the user.
    /// The database should store information attached to class ids. Each
    ///   class id should map to an atomic, visually recognizable element. This
    ///   definition is intentionally vague to allow extreme flexibility. The
    ///   elements could be classes in a pixel segmentation algorithm, object classes
    ///   in a detector, different people's faces in a face detection algorithm, etc.
    ///   Vision pipelines report results in terms of numeric IDs, which map into
    ///   this  database.
    /// The information stored in this database is, again, left up to the user. The
    ///   database could be as simple as a map from ID to class name, or it could
    ///   include information such as object meshes or colors to use for
    ///   visualization.
    pub database_location: std::string::String,

    /// Metadata database version. This counter is incremented
    ///   each time the pipeline begins using a new version of the database (useful
    ///   in the case of online training or user modifications).
    ///   The counter value can be monitored by listeners to ensure that the pipeline
    ///   and the listener are using the same metadata.
    pub database_version: i32,

}



impl Default for VisionInfo {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::VisionInfo::default())
  }
}

impl rosidl_runtime_rs::Message for VisionInfo {
  type RmwMsg = super::msg::rmw::VisionInfo;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        method: msg.method.as_str().into(),
        database_location: msg.database_location.as_str().into(),
        database_version: msg.database_version,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
        method: msg.method.as_str().into(),
        database_location: msg.database_location.as_str().into(),
      database_version: msg.database_version,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      method: msg.method.to_string(),
      database_location: msg.database_location.to_string(),
      database_version: msg.database_version,
    }
  }
}


