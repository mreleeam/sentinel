import json
from pathlib import Path
from django.contrib.auth import login
from django.contrib.auth.decorators import login_required

from django.http import JsonResponse
from django.shortcuts import render


METRICS_FILE = Path("/var/lib/sentinel/metrics.json")


def read_metrics():
    if not METRICS_FILE.exists():
        return None

    try:
        with METRICS_FILE.open("r", encoding="utf-8") as file:
            return json.load(file)

    except (OSError, json.JSONDecodeError, TypeError):
        return None


@login_required
def home(request):
    metrics = read_metrics()

    return render(
        request,
        "home.html",
        {
            "agent_running": metrics is not None,
            "metrics": metrics,
        },
    )


@login_required
def metrics_api(request):
    metrics = read_metrics()

    if metrics is None:
        return JsonResponse(
            {
                "agent_running": False,
            },
            status=503,
        )

    response_data = {
        "hostname": metrics.get("hostname", "unknown"),
        "uptime": metrics.get("uptime", "unknown"),
        "cpu": metrics.get("cpu", {}),
        "memory": metrics.get("memory", {}),
        "swap": metrics.get("swap", {}),
        "disk": metrics.get("disk", {}),
        "network": metrics.get("network", {}),
    }

    return JsonResponse(response_data)


@login_required
def processes_api(request):
    metrics = read_metrics()

    if metrics is None:
        return JsonResponse(
            {
                "agent_running": False,
                "processes": [],
            },
            status=503,
        )

    return JsonResponse(
        {
            "agent_running": True,
            "processes": metrics.get("processes", []),
        }
    )
