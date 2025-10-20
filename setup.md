# PalPalette Edge Device Registration Guide

## Overview

This document outlines the complete registration and setup process for PalPalette edge devices (ESP32). The process involves HTTP REST API calls for registration/status updates and WebSocket connections for real-time communication.

## Base URLs

- **HTTP API**: `http://YOUR_BACKEND_IP:3000`
- **WebSocket**: `ws://YOUR_BACKEND_IP:3001/ws`

---

## Step-by-Step Registration Process

### 1. Device Startup & Self-Registration

When your device boots up and connects to WiFi, immediately register with the backend:

**Endpoint**: `POST /devices/register`  
**Public**: Yes (no authentication required)

**Payload**:

```json
{
  "macAddress": "00:1B:44:11:3A:B7",
  "ipAddress": "192.168.1.100",
  "deviceType": "esp32c3",
  "firmwareVersion": "1.2.3",
  "lightingSystemType": "ws2812",
  "lightingHostAddress": "192.168.1.101",
  "lightingPort": 80,
  "lightingAuthToken": "optional-auth-token",
  "lightingCustomConfig": {
    "ledCount": 60,
    "pin": 2
  }
}
```

**Required Fields**:

- `macAddress` (format: XX:XX:XX:XX:XX:XX)

**Optional Fields**:

- `ipAddress`, `deviceType`, `firmwareVersion`
- Lighting system configuration (if known during setup)

**Response**:

```json
{
  "device": {
    "id": "b1a4c96c-7833-4d60-8528-34ff392c41e4",
    "name": "ESP32C3-1A:B7",
    "status": "unclaimed",
    "pairingCode": "ABC123",
    "pairingCodeExpiresAt": "2025-10-13T10:38:37.000Z"
  },
  "pairingCode": "ABC123"
}
```

**Important**: Store the `device.id` (UUID) - this is your unique device identifier for all future API calls.

### 2. Establish WebSocket Connection

Immediately after registration, connect to the WebSocket server:

**URL**: `ws://YOUR_BACKEND_IP:3001/ws`

**Initial Connection Message**:

```json
{
  "event": "deviceConnected",
  "data": {
    "deviceId": "b1a4c96c-7833-4d60-8528-34ff392c41e4",
    "macAddress": "00:1B:44:11:3A:B7",
    "ipAddress": "192.168.1.100",
    "firmwareVersion": "1.2.3"
  }
}
```

### 3. Continuous Status Updates

Send regular status updates to keep the device "alive" in the system:

**Endpoint**: `PUT /devices/{deviceId}/status`  
**Public**: Yes  
**Frequency**: Every 30-60 seconds

**Payload**:

```json
{
  "isOnline": true,
  "isProvisioned": true,
  "ipAddress": "192.168.1.100",
  "firmwareVersion": "1.2.3",
  "wifiRSSI": -45,
  "freeHeap": 45000,
  "uptime": 3600
}
```

### 4. WebSocket Heartbeat

Send WebSocket ping frames every 30 seconds to maintain connection and update `lastSeenAt`:

```javascript
// Send ping every 30 seconds
setInterval(() => {
  if (websocket.readyState === WebSocket.OPEN) {
    websocket.ping();
  }
}, 30000);
```

### 5. Handle User Claiming

Listen for the `deviceClaimed` event via WebSocket:

**Incoming Message**:

```json
{
  "event": "deviceClaimed",
  "data": {
    "setupToken": "user-setup-token",
    "userEmail": "user@example.com",
    "userName": "John Doe"
  }
}
```

**Response**: Acknowledge the claim and update your internal state.

---

## Device States & Status Flow

1. **Unclaimed** → Device registered but not owned by any user
2. **Claimed** → User has claimed the device using pairing code
3. **Online** → Device is connected and responsive
4. **Offline** → Device hasn't sent status updates recently

---

## WebSocket Message Types

### Outgoing (Device → Backend)

#### Device Status Updates

```json
{
  "event": "deviceStatus",
  "data": {
    "deviceId": "your-device-uuid",
    "isOnline": true,
    "firmwareVersion": "1.2.3",
    "ipAddress": "192.168.1.100",
    "wifiRSSI": -45,
    "freeHeap": 45000,
    "uptime": 3600
  }
}
```

#### Lighting System Status

```json
{
  "event": "lightingSystemStatus",
  "data": {
    "deviceId": "your-device-uuid",
    "status": "working",
    "systemType": "ws2812",
    "lastTest": "2025-10-13T10:30:00.000Z"
  }
}
```

#### User Action Required

```json
{
  "event": "userActionRequired",
  "data": {
    "deviceId": "your-device-uuid",
    "action": "authenticate_hue",
    "instructions": "Press the button on your Philips Hue bridge",
    "timeout": 30000,
    "type": "lighting_authentication"
  }
}
```

### Incoming (Backend → Device)

#### Device Claimed Notification

```json
{
  "event": "deviceClaimed",
  "data": {
    "setupToken": "user-setup-token",
    "userEmail": "user@example.com",
    "userName": "John Doe"
  }
}
```

#### Color Palette Commands

```json
{
  "event": "colorPalette",
  "data": {
    "messageId": "msg-uuid",
    "senderId": "user-uuid",
    "senderName": "Alice",
    "colors": ["#FF0000", "#00FF00", "#0000FF"],
    "timestamp": 1697193600000
  }
}
```

#### Factory Reset Command

```json
{
  "event": "factoryReset",
  "data": {
    "deviceId": "your-device-uuid",
    "timestamp": 1697193600000,
    "message": "Device has been reset by user. Returning to setup mode."
  }
}
```

#### Lighting System Test Request

```json
{
  "event": "testLightingSystem",
  "data": {
    "deviceId": "your-device-uuid",
    "timestamp": 1697193600000
  }
}
```

---

## Lighting System Status Values & Frontend Usage

When the device attempts to connect to a lighting system (e.g., Nanoleaf, WLED, Hue), it sends a `lightingSystemStatus` event to the backend. The backend stores the full status payload in the `lightingStatusDetails` field, which is available via the REST endpoint:

**GET /devices/{deviceId}/lighting/status**

### Example Response

```json
{
  "lightingSystemType": "nanoleaf",
  "lightingStatus": "authentication_required",
  "lightingStatusDetails": {
    "deviceId": "...",
    "status": "authentication_required",
    "systemType": "nanoleaf",
    "details": "Press the button on your Nanoleaf controller.",
    "lastTest": "2025-10-13T10:30:00.000Z"
  },
  ...other fields...
}
```

### Status Values

- `working`: Lighting system is connected and operational.
- `authentication_required`: User must perform an action (e.g., press a button on the controller). See `details` for instructions.
- `error`: Connection failed or system is not responding. See `details` for error info.
- `unknown`: Status is not determined.

### Frontend Usage

1. **Poll** `/devices/{deviceId}/lighting/status` to get the latest status and details.
2. **Display** the `lightingStatus` and any `details` from `lightingStatusDetails` to the user.
3. **Guide** the user through any required actions (e.g., show instructions if `authentication_required`).
4. **Show errors** if status is `error` and provide troubleshooting info from `details`.
5. **Show success** if status is `working`.

#### Example UI Logic

- If `lightingStatus` is `authentication_required`, show a prompt: "Press the button on your Nanoleaf controller."
- If `lightingStatus` is `error`, show: "Could not connect to lighting system: [details]"
- If `lightingStatus` is `working`, show: "Lighting system connected!"

---

## Error Handling

### HTTP API Errors

- **400 Bad Request**: Invalid payload or validation error
- **404 Not Found**: Device not found
- **409 Conflict**: Device already exists (for registration)
- **500 Server Error**: Database or internal server error

### WebSocket Connection Issues

- **Connection Lost**: Implement reconnection logic with exponential backoff
- **Invalid Messages**: Backend will send error responses in JSON format
- **Authentication**: WebSocket connections are currently public (no auth required)

---

## Implementation Checklist

### Required Implementation:

- [ ] HTTP POST to `/devices/register` on startup
- [ ] Store received device UUID for all future API calls
- [ ] WebSocket connection to `/ws` endpoint
- [ ] Send initial "deviceConnected" message via WebSocket
- [ ] Regular status updates via PUT `/devices/{id}/status`
- [ ] WebSocket ping/pong heartbeat every 30 seconds
- [ ] Handle incoming WebSocket messages (especially "deviceClaimed")
- [ ] Implement reconnection logic for WebSocket

### Optional Implementation:

- [ ] Lighting system status reporting
- [ ] User action required notifications
- [ ] Color palette display functionality
- [ ] Factory reset handling
- [ ] Lighting system test responses

---

## Example Device Flow

```
1. Device boots → WiFi connection established
2. HTTP POST /devices/register → Receive device UUID + pairing code
3. WebSocket connect to /ws → Send deviceConnected message
4. Display pairing code to user (LED pattern, screen, etc.)
5. Start status update loop (every 30-60 seconds)
6. Start WebSocket heartbeat (every 30 seconds)
7. Wait for "deviceClaimed" message via WebSocket
8. Device is now ready to receive color palettes and commands
```

---

## Security Notes

- Device registration and status endpoints are currently **public** (no authentication)
- WebSocket connections are **public** and identified by device UUID
- Pairing codes expire after **30 minutes**
- Store device UUID securely - it's your permanent identity

---

## Support & Debugging

- Check device appears in: `GET /devices/discover/unpaired`
- Verify connection status: `GET /devices/{deviceId}/pairing-info`
- Monitor backend logs for WebSocket connection status
- Test connectivity with: `GET /devices/by-mac/{macAddress}`
