# 🏎️ Modular 1:18 Scale F1 RC Chassis Platform

A modular 1:18 scale open-wheel racing chassis powered by an ESP32-S3 and designed in Fusion 360 to adapt across F1 eras, including experimental multi-axle setups such as the Tyrrell P34 and Williams FW07D.

---

## 🛠️ Project Status & Development Roadmap

> **Current Stage:** Active CAD R&D & Prototyping Phase

* [x] **Main Chassis Plate Design:** Fusion 360 baseline geometry complete.
* [x] **Rear Drivetrain & Suspension:** Motor mount, spur gear assembly, and rear axle complete.
* [x] **Electronics & Firmware:** ESP32-S3 receiver code with ICM-42688-P IMU integration and safety failsafes complete.
* [ ] **Steering Assembly:** Optimizing Ackermann geometry for 1:18 scale.
* [ ] **Physical Assembly:** Pending micro-grant funding for hardware components and the high-speed motor/ESC setup.

---

## 📸 CAD Images

*Figure 1: Work-in-progress Fusion 360 main chassis assembly.*
<img width="1066" height="682" alt="image" src="https://github.com/user-attachments/assets/0aedbffc-e7ae-4f0f-9c79-5f89b64117cc" />
<img width="1098" height="699" alt="image" src="https://github.com/user-attachments/assets/d69dd5f1-3c0b-48ad-ae61-0e9f44b85a74" />
<img width="376" height="751" alt="image" src="https://github.com/user-attachments/assets/48b42cc2-cd84-411b-9736-bb84bd6ea039" />

---

## 🔌 Electronics & Wiring Diagram


*Figure 2: Wiring schematic linking the ESP32-S3, ICM-42688-P IMU, 30A brushed ESC, Battery, motor and digital steering servo.*
<img width="894" height="698" alt="image" src="https://github.com/user-attachments/assets/c5f58fe4-4c0f-4d8f-a61e-b7d8f7251ed0" />


---

## 🛒 Bill of Materials (BOM)
*I will try and source the TPU myself to be able to get in HackClub Forge tier 3 under 0-100$.*

| Component                              |          Qty. |       Price | Link                                                                               |
| -------------------------------------- | ------------: | ----------: | ---------------------------------------------------------------------------------- |
| ESP32-S3 Dev Board                     |             1 |       €5.95 | [AliExpress](https://www.aliexpress.com/item/1005005757810089.html)                |
| ICM-42688-P Purple                     |             1 |      €15.36 | [AliExpress](https://www.aliexpress.com/item/1005010173401329.html)                |
| 30A Micro Brushed ESC                  |             1 |       €4.83 | [AliExpress](https://www.aliexpress.com/item/1005006397492790.html)                |
| FK130 High-Speed Motor                 |             1 |       €6.38 | [AliExpress](https://www.aliexpress.com/item/1005007476143938.html)                |
| TOUCAN RC HOBBY 800mAh 2S LiPo Battery |             1 |      €20.91 | [Amazon](https://www.amazon.com/TOUCAN-RC-HOBBY-Skid-Steer-Airplane/dp/B0CCN8S5MR) |
| RCXAZ 2g Micro Digital Servo           |             1 |       €8.44 | [AliExpress](https://www.aliexpress.com/item/1005010424552930.html)                |
| MR115 2RS 5×11×4mm Bearings            |         1 set |       €7.81 | [AliExpress](https://www.aliexpress.com/item/1005009331840004.html)                |
| ColorFabb TPU varioShore 95A           |   1.75mm 700g |      €72.48 | [ColorFabb](https://colorfabb.com/varioshore-tpu-black)                            |
| Dupont Cable Ribbon Jumper Wire Kit    | Set of 90 pcs |       €6.99 | [AliExpress](https://www.aliexpress.com/item/1005003269498051.html)                |
| AliExpress Shipping + Import Fees      |             — |      €26.05 | —                                                                                  |
| **Total**                              |               | **€166.44** |                                                                                    |

---

## 💻 Firmware

The ESP32-S3 receiver firmware, including ICM-42688-P IMU integration, safety failsafes, and ESP-NOW control, is located in `/code/Car code.ino`.
