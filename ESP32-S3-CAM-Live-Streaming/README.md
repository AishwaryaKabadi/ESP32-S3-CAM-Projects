# ESP32-S3-CAM Live Streaming

Stream live video from your ESP32-S3-CAM and access it remotely using ngrok.

## 📹 Project

In this project, the ESP32-S3-CAM connects to Wi-Fi and creates a live camera stream.

Using ngrok, you can access the camera remotely from your phone through the internet.

## 🛠️ What You Need

* ESP32-S3-CAM
* USB cable
* Wi-Fi connection
* Computer
* ngrok account

## 🚀 How It Works

ESP32-S3-CAM → Wi-Fi → ngrok → Internet → Phone

## ⚙️ Setup

1. Open the Arduino project.
2. Add your Wi-Fi name and password.
3. Upload the code to your ESP32-S3-CAM.
4. Open the Serial Monitor.
5. Copy the camera's IP address.
6. Test the live camera stream.
7. Start ngrok using your ESP32-S3-CAM IP address.

Example:

```bash
ngrok http YOUR_ESP32_IP:80
```

Example:

```bash
ngrok http 192.168.1.100:80
```

8. Open the ngrok URL on your phone.

## ⚠️ Security

Do not expose your camera publicly without proper security and authentication.

## 🎥 YouTube Tutorial

Watch the complete tutorial on my YouTube channel.

## 📄 License

This project is for educational purposes.
