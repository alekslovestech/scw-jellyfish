const messageContainer = document.getElementById("message-container");
const deviceHostInput = document.getElementById("device-host-input") as HTMLInputElement | null;
const refreshStatusButton = document.getElementById("refresh-status-button") as HTMLButtonElement | null;
const deviceNameElem = document.getElementById("device-name");
const firmwareVersionElem = document.getElementById("firmware-version");
const chipIdElem = document.getElementById("chip-id");
const macAddressElem = document.getElementById("mac-address");
const wifiStatusElem = document.getElementById("wifi-status");
const hostnameElem = document.getElementById("hostname");
const identifyButton = document.getElementById("identify-button") as HTMLButtonElement;
const renameForm = document.getElementById("rename-form") as HTMLFormElement;
const deviceNameInput = document.getElementById("device-name-input") as HTMLInputElement;
const updateForm = document.getElementById("update-form") as HTMLFormElement;
const firmwareFileInput = document.getElementById("firmware-file") as HTMLInputElement;

let deviceBaseUrl = "";

const DEFAULT_DEVICE_HOST = "http://device.local";
const HOST_STORAGE_KEY = "jelly-device-host";

function loadStoredHost(): string {
  try {
    return localStorage.getItem(HOST_STORAGE_KEY) ?? "";
  } catch {
    return "";
  }
}

function storeHost(host: string) {
  try {
    localStorage.setItem(HOST_STORAGE_KEY, host);
  } catch {
    /* localStorage unavailable (private mode etc.) — ignore */
  }
}

interface WifiStatusResponse {
  deviceName: string;
  firmwareVersion: string;
  chipId: string;
  macAddress: string;
  wifiStatus: string;
  hostname: string;
}

function showMessage(message: string, isError = false) {
  if (!messageContainer) return;
  messageContainer.innerHTML = `<div class="msg" style="background:${isError ? "#ffe8e8" : "#eef4ff"};border-color:${isError ? "#f5c2c2" : "#c6dcff"};color:${isError ? "#63171b" : "#102a43"};">${message}</div>`;
}

function getDeviceUrl(path: string): string {
  const base = deviceBaseUrl || DEFAULT_DEVICE_HOST;
  return `${base.replace(/\/+$/, "")}${path}`;
}

async function fetchStatus() {
  const host = deviceHostInput?.value.trim() || deviceBaseUrl;
  if (!host) {
    showMessage("Enter the device host above, then press “Refresh status”.");
    return;
  }
  deviceBaseUrl = host;
  storeHost(host);

  try {
    const resp = await fetch(getDeviceUrl("/wifi/status"));
    if (!resp.ok) throw new Error(`Status fetch failed: ${resp.status}`);
    const data = (await resp.json()) as WifiStatusResponse;

    if (deviceNameElem) deviceNameElem.textContent = data.deviceName;
    if (firmwareVersionElem) firmwareVersionElem.textContent = data.firmwareVersion;
    if (chipIdElem) chipIdElem.textContent = data.chipId;
    if (macAddressElem) macAddressElem.textContent = data.macAddress;
    if (wifiStatusElem) wifiStatusElem.textContent = data.wifiStatus;
    if (hostnameElem) hostnameElem.textContent = data.hostname;
    if (deviceNameInput) deviceNameInput.value = data.deviceName;
    showMessage("Device status loaded.");
  } catch (err) {
    showMessage(`Unable to load device status. ${err instanceof Error ? err.message : ""}`, true);
  }
}

async function identifyDevice() {
  if (!identifyButton) return;
  identifyButton.disabled = true;
  try {
    const resp = await fetch(getDeviceUrl("/wifi/identify"), { method: "POST" });
    if (!resp.ok) throw new Error(`Identify request failed: ${resp.status}`);
    showMessage("Identify request sent.");
  } catch (err) {
    showMessage(`Unable to request identify. ${err instanceof Error ? err.message : ""}`, true);
  } finally {
    identifyButton.disabled = false;
  }
}

async function renameDevice(event: Event) {
  event.preventDefault();
  if (!deviceNameInput) return;

  const name = deviceNameInput.value.trim();
  if (!name) {
    showMessage("Please enter a device name.", true);
    return;
  }

  try {
    // urlencoded (not multipart) so the ESP32 WebServer populates server.arg("name")
    const body = new URLSearchParams();
    body.append("name", name);

    const resp = await fetch(getDeviceUrl("/wifi/rename"), {
      method: "POST",
      body,
    });
    if (!resp.ok) throw new Error(`Rename request failed: ${resp.status}`);
    showMessage("Rename request sent. Device will reboot shortly.");
  } catch (err) {
    showMessage(`Unable to rename device. ${err instanceof Error ? err.message : ""}`, true);
  }
}

async function uploadFirmware(event: Event) {
  event.preventDefault();
  if (!firmwareFileInput || !firmwareFileInput.files?.length) {
    showMessage("Select a firmware .bin file first.", true);
    return;
  }

  const file = firmwareFileInput.files[0];
  const formData = new FormData();
  formData.append("update", file);

  const submitButton = updateForm.querySelector("input[type=submit]") as HTMLInputElement | null;
  if (submitButton) submitButton.disabled = true;

  try {
    const resp = await fetch(getDeviceUrl("/wifi/update"), {
      method: "POST",
      body: formData,
    });
    if (!resp.ok) throw new Error(`Firmware upload failed: ${resp.status}`);
    showMessage("Firmware upload sent. Device should reboot after install.");
  } catch (err) {
    showMessage(`Firmware upload failed. ${err instanceof Error ? err.message : ""}`, true);
  } finally {
    if (submitButton) submitButton.disabled = false;
  }
}

if (identifyButton) identifyButton.addEventListener("click", identifyDevice);
if (renameForm) renameForm.addEventListener("submit", renameDevice);
if (updateForm) updateForm.addEventListener("submit", uploadFirmware);
if (refreshStatusButton) refreshStatusButton.addEventListener("click", fetchStatus);

const storedHost = loadStoredHost();
if (storedHost) {
  deviceBaseUrl = storedHost;
  if (deviceHostInput) deviceHostInput.value = storedHost;
  fetchStatus();
} else {
  showMessage("Enter the device host above, then press “Refresh status” to begin.");
}
