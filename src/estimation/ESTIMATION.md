### State estimation
Combine those measurements to answer: 
    **"Where am I, how am I oriented, and how am I moving?"**

Output is something like

$$
x=[position,\ velocity,\ orientation,\ angular\ velocity]
$$

This is where an EKF, VIO, etc. come in. If GPS is unavailable, VIO becomes especially important.