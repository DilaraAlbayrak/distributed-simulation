# 700105-2425-FinalLab

The purpose of this assessment is to design and implement an efficient network-based simulation of a series of physics-based scenarios. 

This project involves building a distributed, peer-to-peer physics simulation, where objects from multiple computers interact in real time within a shared physics-based environment. The simulation integrates multithreaded physics (gravity, collision detection and response), synchronised global state, and adjustable simulation frequencies, all developed using DirectX 11 and Winsock 2.

- Built on a peer-to-peer (P2P) model, explicitly avoiding a client-server setup. Google Flatbuffers for efficient, low-overhead data serialisation of network messages.
- Features a state-smoothing mechanism that interpolates object positions to mitigate visual judder from network latency.
- Leverages modern C++ concurrency primitives including std::thread, std::mutex, std::shared_mutex, std::condition_variable, and std::barrier.
- Implements multiple integration methods, including Semi-Implicit Euler, Midpoint, and Runge-Kutta 4 (RK4), for calculating object motion.
- Realistic collision responses based on momentum conservation, with advanced features like material-based elasticity (bounciness) and friction (static and dynamic) managed via lookup tables.

https://github.com/user-attachments/assets/204767b6-a1e6-4949-ae26-74f8a7fd5307

