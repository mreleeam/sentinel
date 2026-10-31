"use strict";

const METRICS_REFRESH_INTERVAL = 1000;
const PROCESS_REFRESH_INTERVAL = 1000;


const processToggle = document.getElementById("process-toggle");
const processContainer = document.getElementById("process-container");
const processToggleIcon = document.getElementById("process-toggle-icon");
const processToggleText = document.getElementById("process-toggle-text");
const processTableBody = document.getElementById("process-table-body");
const processCount = document.getElementById("process-count");


function getNumber(value, fallback = 0) {
  const number = Number(value);

  return Number.isFinite(number)
    ? number
    : fallback;
}


function formatPercentage(value) {
  return getNumber(value).toFixed(2);
}


function formatGB(value) {
  return `${getNumber(value).toFixed(2)} GB`;
}


function formatMB(value) {
  return `${getNumber(value).toFixed(2)} MB`;
}


function formatSpeed(value) {
  return `${getNumber(value).toFixed(2)} MB/s`;
}


function clamp(value) {
  return Math.max(
    0,
    Math.min(100, getNumber(value))
  );
}


function setText(id, value) {

  const element = document.getElementById(id);

  if (element) {
    element.textContent = value;
  }

}


function updateBar(id, value) {

  const element = document.getElementById(id);

  if (!element) {
    return;
  }

  element.style.width = `${clamp(value)}%`;

}


function escapeHtml(value) {

  const element = document.createElement("div");

  element.textContent = value ?? "";

  return element.innerHTML;

}


async function refreshMetrics() {

  try {

    const response = await fetch(
      `/api/metrics/?_=${Date.now()}`,
      {
        method: "GET",
        cache: "no-store",
        headers: {
          "Accept": "application/json"
        }
      }
    );


    if (!response.ok) {
      throw new Error(
        `HTTP ${response.status}`
      );
    }


    const data = await response.json();


    if (data.agent_running === false) {

      setAgentOffline();

      return;

    }


    updateMetrics(data);

    setAgentOnline();


  } catch (error) {

    console.error(
      "[Sentinel] Metrics refresh failed:",
      error
    );

    setAgentOffline();

  }

}


function updateMetrics(data) {

  setText(
    "hostname",
    data.hostname ?? "unknown"
  );


  setText(
    "uptime",
    data.uptime ?? "unknown"
  );


  const cpuUsage =
    data.cpu?.usage ?? 0;


  setText(
    "cpu-value",
    formatPercentage(cpuUsage)
  );


  updateBar(
    "cpu-bar",
    cpuUsage
  );


  const memory =
    data.memory ?? {};


  setText(
    "memory-value",
    formatPercentage(memory.usage)
  );


  updateBar(
    "memory-bar",
    memory.usage
  );


  setText(
    "memory-details",
    `${formatGB(memory.used_gb)} / ${formatGB(memory.total_gb)}`
  );


  setText(
    "memory-available",
    formatGB(memory.available_gb)
  );


  const swap =
    data.swap ?? {};


  setText(
    "swap-value",
    formatPercentage(swap.usage)
  );


  updateBar(
    "swap-bar",
    swap.usage
  );


  setText(
    "swap-details",
    `${formatGB(swap.used_gb)} / ${formatGB(swap.total_gb)}`
  );


  const disk =
    data.disk ?? {};


  setText(
    "disk-value",
    formatPercentage(disk.usage)
  );


  updateBar(
    "disk-bar",
    disk.usage
  );


  setText(
    "disk-details",
    `${formatGB(disk.available_gb)} AVAILABLE`
  );



  const network =
    data.network ?? {};


  setText(
    "network-interface",
    network.interface ?? "unknown"
  );


  setText(
    "network-rx",
    formatMB(network.rx_mb)
  );


  setText(
    "network-tx",
    formatMB(network.tx_mb)
  );


  setText(
    "network-rx-speed",
    formatSpeed(network.rx_mb_s)
  );


  setText(
    "network-tx-speed",
    formatSpeed(network.tx_mb_s)
  );


  const now = new Date();

  setText(
    "last-update",
    now.toLocaleTimeString()
  );

}


function setAgentOnline() {

  const status =
    document.getElementById("agent-status");

  if (!status) {
    return;
  }

  status.textContent = "● AGENT ONLINE";

  status.classList.remove(
    "agent-status-offline"
  );

}


function setAgentOffline() {

  const status =
    document.getElementById("agent-status");

  if (!status) {
    return;
  }

  status.textContent = "● AGENT OFFLINE";

  status.classList.add(
    "agent-status-offline"
  );

}


async function refreshProcesses() {

  try {

    const response = await fetch(
      `/api/processes/?_=${Date.now()}`,
      {
        method: "GET",
        cache: "no-store",
        headers: {
          "Accept": "application/json"
        }
      }
    );


    if (!response.ok) {
      throw new Error(
        `HTTP ${response.status}`
      );
    }


    const data = await response.json();


    if (!data.agent_running) {

      renderProcesses([]);

      return;

    }


    renderProcesses(
      Array.isArray(data.processes)
        ? data.processes
        : []
    );


  } catch (error) {

    console.error(
      "[Sentinel] Process refresh failed:",
      error
    );

  }

}


function renderProcesses(processes) {

  if (!processTableBody) {
    return;
  }


  processTableBody.replaceChildren();


  if (processCount) {

    processCount.textContent =
      `${processes.length} ${processes.length === 1
        ? "PROCESS"
        : "PROCESSES"
      }`;

  }


  for (const process of processes) {

    const row =
      document.createElement("tr");


    const pid =
      getNumber(process.pid, "-");


    const name =
      escapeHtml(process.name ?? "-");


    const user =
      escapeHtml(process.user ?? "-");


    const cpu =
      formatPercentage(process.cpu_usage);


    const memory =
      formatMB(process.memory_mb);


    const exe =
      escapeHtml(process.exe_path ?? "-");


    row.innerHTML = `
            <td class="process-pid">
                ${pid}
            </td>

            <td class="process-name">
                ${name}
            </td>

            <td>
                ${user}
            </td>

            <td class="process-cpu">
                ${cpu}%
            </td>

            <td class="process-memory">
                ${memory}
            </td>

            <td
                class="process-exe"
                title="${exe}"
            >
                ${exe}
            </td>
        `;


    processTableBody.appendChild(row);

  }

}


if (processToggle) {

  processToggle.addEventListener(
    "click",
    async () => {

      const isClosed =
        processContainer.hidden;


      if (!isClosed) {

        processContainer.hidden = true;

        processToggle.classList.remove(
          "process-toggle-active"
        );

        processToggleIcon.textContent = "+";

        processToggleText.textContent =
          "PROCESSES";

        return;

      }


      processToggle.disabled = true;

      processToggleIcon.textContent = "…";

      processToggleText.textContent =
        "LOADING";


      await refreshProcesses();


      processContainer.hidden = false;

      processToggle.classList.add(
        "process-toggle-active"
      );

      processToggleIcon.textContent = "−";

      processToggleText.textContent =
        "PROCESSES";

      processToggle.disabled = false;

    }
  );

}


async function metricsLoop() {

  await refreshMetrics();

}


async function processLoop() {

  if (
    processContainer &&
    !processContainer.hidden
  ) {

    await refreshProcesses();

  }

}


metricsLoop();


setInterval(
  metricsLoop,
  METRICS_REFRESH_INTERVAL
);


setInterval(
  processLoop,
  PROCESS_REFRESH_INTERVAL
);
