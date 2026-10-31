# Sentinel

**v1.0**

A small Linux monitoring agent with a web dashboard.

The agent is a C program that watches CPU, memory, swap, disk, network, and
processes, and writes what it sees to a JSON file. The dashboard is a Django
app that reads that file and shows it in a browser. Both run as systemd
services and start on boot.

I built this as a learning project, so the design favors simplicity over
features. No database for metrics, no external services, no agents to install
on other machines. Just one binary, one file, one dashboard.

---

## How it works

```
┌────────────────────┐        ┌──────────────────────┐        ┌────────────────────┐
│   sentinel-agent   │ writes │  /var/lib/sentinel/  │ reads  │  Django dashboard  │
│   (C, systemd)     │ ─────► │     metrics.json     │ ─────► │  (systemd, :8000)  │
└────────────────────┘        └──────────────────────┘        └────────────────────┘
```

The agent collects metrics every interval (configurable, one second by
default) and appends them to a JSON file. The dashboard reads that file on
each request and renders it. User accounts and Django's own state live in
SQLite, separate from the metrics.

---

## Requirements

You'll need a Linux distribution that uses systemd. That covers Ubuntu,
Debian, Fedora, RHEL, Arch, openSUSE, and most others. Distributions that use
another init system (Alpine, Devuan, Artix) won't work, at least not without
changes.

You'll also need:

- `sudo` access
- Network access during installation, because the installer runs `pip install`
- The dependencies below

### Dependencies

The installer does **not** install anything automatically. Install these
first, with whatever package manager your distro uses.

| Dependency | What it's for |
|---|---|
| `gcc`, `make`, `cmake` | Build the C agent |
| `python3` | Run Django and the installer itself |
| Python `venv` module | Create the dashboard's virtual environment |
| `pip` | Install Python packages into the venv |
| `ca-certificates` | Let `pip` reach PyPI over HTTPS |
| `sudo`, `systemctl` | Privileged operations and service management |

Install commands per distribution:

```bash
# Debian / Ubuntu
sudo apt update
sudo apt install build-essential cmake python3 python3-venv python3-pip ca-certificates

# Fedora / RHEL / CentOS
sudo dnf install gcc make cmake python3 python3-pip ca-certificates

# Arch
sudo pacman -S base-devel cmake python python-pip ca-certificates
# sudo isn't installed by default on Arch, so install it first:
#   pacman -S sudo

# openSUSE
sudo zypper install gcc make cmake python3 python3-pip ca-certificates
```

> **A note for Fedora and RHEL.** SELinux is enforcing by default on these
> systems, which blocks systemd from running binaries that live inside a
> home directory. The installer handles this by relabeling the virtualenv
> and agent binaries, so they work under systemd. If `policycoreutils-python-utils`
> (the package that provides `semanage`) isn't installed, the installer falls
> back to `chcon`, which works for the current boot but doesn't survive a
> full filesystem relabel. Install it if you want the fix to be permanent.

---

## Installation

```bash
git clone https://github.com/mreleeam/sentinel.git
cd Sentinel
./install.sh
```

The installer walks through these steps:

1. Checks that every required command is available.
2. Builds the C agent with CMake.
3. Installs the agent to `/usr/local/bin/sentinel-agent`.
4. Creates `/etc/sentinel/` for configuration and `/var/lib/sentinel/` for
   runtime data.
5. Creates a Python virtual environment at `web/.venv` and installs Django,
   Gunicorn, and Whitenoise.
6. Asks you for dashboard credentials.
7. Generates a Django secret key and writes it to `/etc/sentinel/web.env`.
8. Creates the SQLite database and applies migrations.
9. Installs and enables both systemd services.
10. Starts both services and checks that they're actually running.

If any step fails, the installer shows the journal output and exits with a
non-zero status. It won't tell you everything worked if it didn't.

### What it asks you

The installer prompts for three things:

- **Additional hostnames or IPs** for `DJANGO_ALLOWED_HOSTS`. This is
  optional; leave it blank if you'll only access the dashboard from the
  machine itself. Anything you enter here is added to the hostnames that are
  detected automatically.
- **Dashboard username.**
- **Dashboard password**, entered twice.

The detected hostnames (`localhost`, `127.0.0.1`, the system's short and FQDN
hostnames, and every address returned by `hostname -I`) are always included.

---

## Using it

Open the dashboard:

```
http://localhost:8000
```

From another machine:

```
http://<your-host-ip>:8000
```

Log in with the credentials you entered during installation.

To check that both services are running:

```bash
systemctl status sentinel-agent.service sentinel-web.service
```

To follow the logs:

```bash
journalctl -u sentinel-agent.service -f
journalctl -u sentinel-web.service -f
```

### Firewall

The installer doesn't touch your firewall. If you want to reach the dashboard
from another machine, open port `8000` yourself:

```bash
# firewalld (Fedora, RHEL)
sudo firewall-cmd --permanent --add-port=8000/tcp
sudo firewall-cmd --reload

# ufw (Ubuntu, Debian)
sudo ufw allow 8000/tcp
```

---

## Configuration

### The agent

The agent's configuration lives at `/etc/sentinel/config.ini`. It's copied
from `agent/config.ini` on the first install, and never overwritten after
that, so your edits survive reinstalls.

The agent's working directory under systemd is `/var/lib/sentinel/`, which is
also where it writes its state and metrics files. That directory is created
by the installer.

### The dashboard

Django reads its settings from `/etc/sentinel/web.env`, which contains:

- `DJANGO_SECRET_KEY`, generated on first install and kept on reinstall.
- `DJANGO_DEBUG`, always `False`.
- `DJANGO_ALLOWED_HOSTS`, a comma-separated list.

If you need to change `ALLOWED_HOSTS`, edit this file and restart the web
service:

```bash
sudo systemctl restart sentinel-web.service
```

### Your login

The username is stored at `/etc/sentinel/dashboard-user`. The password is
hashed in `web/db.sqlite3`. To change either, re-run `./install.sh` and enter
new values. The installer reuses the existing account when the username
hasn't changed, and deletes the old one when it has.

---

## Uninstalling

```bash
./install.sh -r
```

You'll be asked whether to keep or delete Sentinel's data.

**Keep data.** Preserves `/etc/sentinel/`, `/var/lib/sentinel/`, and
`web/db.sqlite3`. A later reinstall picks up where you left off.

**Delete data.** Removes configuration, credentials, the database, and any
metrics files. You'll be asked to type `DELETE` to confirm.

Either way, the installer stops and disables both services, removes the
service unit files, removes the agent binary, and removes the Python virtual
environment. Your project directory is untouched.

Firewall rules are never changed by the uninstaller.

---

## When something goes wrong

### The agent exits with `status=127`

`127` from systemd usually means it couldn't execute the binary at all.

```bash
ls -la /usr/local/bin/sentinel-agent
cmp agent/build/sentinel-agent /usr/local/bin/sentinel-agent && echo SAME
```

If `cmp` says `DIFFERENT`, the installer installed the wrong file from the
build tree. Re-run `./install.sh`. The current version fails loudly instead
of installing the wrong file silently.

If it says `SAME` but the service still exits with `127`, look at the agent's
own output:

```bash
sudo journalctl -u sentinel-agent.service -n 30 --no-pager
```

### The web service fails on Fedora with "Permission denied"

Almost always SELinux. Check the label on the Python binary:

```bash
ls -Z web/.venv/bin/python
```

If the label ends in `user_home_t`, the relabel didn't apply. Re-run
`./install.sh`.

### The agent runs, but the dashboard shows nothing

Check where the agent is writing:

```bash
sudo journalctl -u sentinel-agent.service -n 30 --no-pager
ls -la /var/lib/sentinel/
```

If it's writing somewhere the dashboard doesn't look, point both at the same
path in `/etc/sentinel/config.ini`.

### "Permission denied" when running the agent manually

The agent needs to read `/proc` and write to `/var/lib/sentinel/`. It's not
meant to run as a regular user. Under systemd it runs as root, which is
correct for this kind of program. For manual testing:

```bash
sudo /usr/local/bin/sentinel-agent
```

### The venv module is missing

```bash
python3 -m venv --help
```

If that fails, install your distro's venv package:

- Debian, Ubuntu: `sudo apt install python3-venv`
- Fedora, RHEL: usually included with `python3`
- Arch: included with `python`

---

## What's in the repo

```
Sentinel/
├── agent/                    C monitoring agent
│   ├── src/                  Agent source
│   ├── config.ini            Default agent configuration
│   ├── CMakeLists.txt        Build configuration
│   └── systemd/
│       └── sentinel-agent.service
├── web/                      Django dashboard
│   ├── manage.py
│   ├── config/               Django project settings
│   ├── dashboard/            Main app
│   ├── requirements.txt
│   └── .venv/                Created by the installer
├── install.sh                Installer and uninstaller
└── README.md
```

---

## A few design notes

**Metrics go in a flat file, not a database.** The agent writes to a JSON
file so it doesn't need to link against a database library or speak a
protocol. That keeps it dependency-free, which makes it easier to audit and
easier to build on machines that don't have a database server running.

**The dashboard uses SQLite.** It only needs to store user accounts and
Django's own tables. SQLite handles that without any extra services, so the
installer doesn't have to start a database daemon.

**The installer checks its own work.** After installing the agent, it
compares the installed binary against the built one with `cmp` and fails if
they differ. After starting both services, it checks they're actually active
and shows the journal if they aren't. This is the difference between
"installation complete" and "installation complete, and here's proof."

**Paths are fixed.** The agent lives at `/usr/local/bin/sentinel-agent`, its
configuration at `/etc/sentinel/`, its data at `/var/lib/sentinel/`. If you
move the project after installing, re-run `./install.sh` so the systemd unit
files pick up the new working directory.

---

## License

MIT. See [LICENSE](LICENSE).
