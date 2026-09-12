# NFS home server — shared `/usr/people` for lucy, arthur and aurora

Server built on `ha` and verified from a macOS client, **12 September 2026**. Every output in
§4 was pasted from the machines. **The IRIX clients have not been tested yet** — the SGIs were
powered off. §6 is written but unproven and says so at the top.

Replaces the UniFi UNAS 2-bay, which forces `root_squash` on NFS with no per-export control.
Root-owned files arrive as `nobody`, which makes it unusable for `/usr/people` homes and for
anything the cluster does as root.

## 0 · What was built

| | |
|---|---|
| Host | `ha`, Intel N100 NUC, Proxmox VE 8.4.12 |
| Container | LXC **105**, hostname `nfs`, `172.28.4.32`, Debian 13, **privileged** |
| Kernel | 6.8.12-14-pve (the host's — LXC shares it) |
| Disk | `/dev/sdb`, Seagate ST2000LM015, 2 TB, 5400 rpm, **SMR** |
| Volume | VG `hd2`, LV `nfsstore`, ext4, 1.7 T, mounted at `/srv` on the host |
| Exports | `/srv/people`, `/srv/cluster`, `/srv/data` — all NFSv3, AUTH_SYS, `no_root_squash` |
| Clients | lucy `172.28.4.8`, arthur `172.28.4.17`, aurora `172.28.4.16`, all IRIX 6.5.30 |

---

## 1 · Storage

`/dev/sdb` was **not** empty. It carried VG `hd2` holding an orphaned LV `vm-102-disk-0`
(1.82 TB), left behind when Plex's media moved to NFS. Three things established it was dead
before anything was removed: `pct config 102` showed no reference to it and no `unused0:`
line, `df` inside 102 showed `/media` coming from `172.28.4.1` over NFS, and a grep of every
guest config found no other claim on it.

```sh
pvs; vgs; lvs
grep -rn hd2 /etc/pve/lxc/ /etc/pve/qemu-server/
lvremove hd2/vm-102-disk-0
lvcreate -n nfsstore -l 90%FREE hd2
mkfs.ext4 -m 1 -L nfsstore /dev/hd2/nfsstore
pvesm remove hd2
```

`-l 90%FREE` leaves roughly 185 GB in the VG as copy-on-write space for LVM snapshots.
`-m 1` drops ext4's reserved blocks from 5% to 1%, returning about 95 GB on a 2 TB data disk.
`pvesm remove hd2` drops the Proxmox storage definition so the GUI stops offering this disk
for guest volumes and consuming that snapshot reserve.

Result, as observed in the container:

```
/dev/mapper/hd2-nfsstore  1.7T   40K  1.6T   1% /srv
```

`/etc/fstab` on `ha`:

```
/dev/hd2/nfsstore  /srv  ext4  defaults,noatime  0 2
```

```sh
mount /srv
mkdir -p /srv/people /srv/cluster /srv/data
umount /srv
chattr +i /srv
mount /srv
```

The `chattr +i` is deliberate. If `/srv` ever fails to mount, the container would otherwise
bind the empty directory underneath it and write homes onto the boot SSD, silently, until it
filled. Immutable-while-unmounted makes that failure impossible instead of invisible.

All three exports are directories on this one ext4 filesystem, not separate volumes. That is
why `df` reports the same filesystem for each of them.

---

## 2 · The container

Build it from the CLI. The GUI's "Unprivileged container" checkbox is easy to miss and there
is no in-place conversion afterwards — the only supported route is `vzdump` then
`pct restore --unprivileged 0`.

```sh
pct create 105 local:vztmpl/debian-13-standard_<ver>_amd64.tar.zst \
  --hostname nfs \
  --unprivileged 0 \
  --cores 2 --memory 1024 --swap 512 \
  --rootfs local-lvm:8 \
  --net0 name=eth0,bridge=vmbr0,ip=172.28.4.32/24,gw=172.28.4.1 \
  --onboot 1 \
  --mp0 /srv,mp=/srv
```

Two raw keys appended to `/etc/pve/lxc/105.conf`:

```
lxc.apparmor.profile: unconfined
lxc.mount.auto: proc:rw sys:rw
```

Both are required and each fixes a different failure:

- **`apparmor.profile: unconfined`** lets the container mount filesystem types at all.
- **`mount.auto: proc:rw`** lets it mount anything *under* `/proc`. Without it,
  `proc-fs-nfsd.mount` fails with `mount: /proc/fs/nfsd: cannot mount nfsd read-only`,
  and `nfs-server` then fails as a dependency. This is the error you will actually see.

The Plex container (102) carries the same `lxc.mount.auto` line for the same underlying reason.

On the host, once:

```sh
echo nfsd > /etc/modules-load.d/nfsd.conf
modprobe nfsd
```

Raw `lxc.*` keys are read at container start, so changes need a full `pct stop` then
`pct start`. A reboot from inside the container will not pick them up and produces the
identical error, which wastes a cycle.

**Privileged is not optional.** On a privileged container a bind mount passes UIDs through
1:1, so the ownership a client writes is the ownership on disk. Unprivileged shifts everything
by 100000 and the IRIX UIDs would land somewhere useless.

---

## 3 · NFS configuration

Inside the container:

```sh
apt update && apt install -y nfs-kernel-server
```

`/etc/nfs.conf`:

```
[nfsd]
vers3=y
vers4=n
udp=y
tcp=y
threads=8
[mountd]
port=20048
[statd]
port=32765
[lockd]
port=32767
```

NFSv4 is **off on purpose**. v4 maps identities by name through `idmapd`; v3 with AUTH_SYS
passes raw numeric UIDs and GIDs, which is what lets the server hold IRIX ownership without
knowing any IRIX usernames. See §5.

The fixed `mountd` / `statd` / `lockd` ports exist so firewall rules stay stable across
restarts rather than chasing whatever rpcbind hands out.

`/etc/exports`:

```
/srv/people   172.28.4.0/24(rw,sync,no_root_squash,no_subtree_check,sec=sys)
/srv/cluster  172.28.4.0/24(rw,sync,no_root_squash,no_subtree_check,sec=sys)
/srv/data     172.28.4.0/24(rw,sync,no_root_squash,no_subtree_check,sec=sys)
```

To apply changes to `/etc/exports`, **`exportfs -ra`**. It re-reads the file and applies the
difference without restarting the daemon, so mounted clients never notice. Do not
`systemctl restart nfs-server` for an export change; that drops clients mid-operation.

```sh
systemctl enable --now nfs-server
exportfs -ra
```

`no_root_squash` on `data` is wider than that share strictly needs. Left on for consistency;
worth revisiting if `data` ever holds anything that does not want root-writable semantics.

---

## 4 · Verification, as observed

On the server:

```
# exportfs -v
/srv/people     172.28.4.0/24(sync,wdelay,hide,no_subtree_check,sec=sys,rw,secure,no_root_squash,no_all_squash)
/srv/cluster    172.28.4.0/24(sync,wdelay,hide,no_subtree_check,sec=sys,rw,secure,no_root_squash,no_all_squash)
/srv/data       172.28.4.0/24(sync,wdelay,hide,no_subtree_check,sec=sys,rw,secure,no_root_squash,no_all_squash)

# cat /proc/fs/nfsd/versions
+3 -4 -4.0 -4.1 -4.2

# rpcinfo -p localhost | grep nfs
    100003    3   tcp   2049  nfs
    100227    3   tcp   2049  nfs_acl
    100003    3   udp   2049  nfs
    100227    3   udp   2049  nfs_acl
```

☐ `no_root_squash` present in every `exportfs -v` option list.
☐ `+3` and every v4 variant negative.
☐ Program 100003 version 3 on **both** udp and tcp.

From a macOS client, **verified**: files created with `sudo` on the mounted share are owned by
`root` on the server, not squashed to `nobody`. That is the specific behaviour the UNAS could
not provide, confirmed end to end from a real client.

Note what is *absent* from `versions`: NFSv2 is not listed at all. Debian ships
`CONFIG_NFSD_V2` disabled and it is deprecated upstream. All three SGIs are 6.5.30 and speak
v3, so this costs nothing here, but a pre-6.5 machine could not use this server and would need
a FreeBSD host instead.

---

## 5 · Identity: no accounts are needed on the server

With v4 off, AUTH_SYS carries numeric UIDs and GIDs over the wire and the server never
consults a name. The server therefore needs **no** `rutger` account, no matching group,
nothing. Ownership is set with raw numbers.

On the SGI:

```sh
id rutger
```

On the server, using those numbers:

```sh
mkdir -p /srv/people/rutger
chown <uid>:<gid> /srv/people/rutger
chmod 755 /srv/people/rutger
```

**All three SGIs must agree on the UID.** Check `grep rutger /etc/passwd` on lucy, arthur and
aurora. A home shared between hosts that disagree looks owned by a stranger on one of them.
Fix the odd host out with `usermod -u` and re-chown its local files *before* migrating.

If you want `ls -l` on the Debian side to show names rather than bare numbers, note that
GID 20 is `dialout` on Debian, so a plain `groupadd -g 20 user` fails. Use
`groupadd -o -g <gid> sgiuser` to allow the duplicate. Purely cosmetic; it changes nothing
about how NFS behaves.

---

## 6 · IRIX clients — NOT YET TESTED

> The SGIs were powered off when the server was built. Everything in this section follows
> from the server config and from the macOS result, but none of it has been run on IRIX.
> Treat it as the plan, not as a record, until the checklist below is ticked.

> **Pasting these blocks into IRIX:** the root shell is `csh`, which does not honour `#`
> comments interactively — it globs what follows and aborts on an unmatched `?` or `*`.
> Type `sh` first and paste into that, or paste one command at a time.

### Test before migrating anything

```sh
showmount -e nfs
mkdir -p /mnt/test
mount -o vers=3,proto=tcp nfs:/srv/people /mnt/test
touch /mnt/test/roottest
ls -ln /mnt/test/roottest
```

☐ `ls -ln` reports uid 0, gid 0. If it says 60001 or `nobody`, root is being squashed
somewhere and nothing below should proceed.
☐ `showmount` answers rather than hanging. A hang points at the container's Proxmox firewall.

IRIX 6.5 speaks NFSv3 over TCP, so `proto=tcp` is the default choice. UDP is registered on the
server as a fallback (§4) if a client ever refuses TCP.

### Seed, then mount

```sh
rsync -aH --numeric-ids /usr/people/ nfs:/srv/people/
```

`--numeric-ids` is not optional. Without it the UIDs are remapped through name lookups on
the far side and the result is a home directory that restores wrong.

`/etc/fstab` on each SGI:

```
nfs:/srv/people /usr/people nfs vers=3,proto=tcp,rw,bg,hard,intr,rsize=32768,wsize=32768 0 0
```

`nfs` must resolve on every SGI. Add it to `/etc/hosts` alongside the lucy/arthur/aurora
entries rather than trusting DNS.

---

## 7 · macOS clients — verified

macOS needs `resvport` and will otherwise fail with a misleading error:

```
mount_nfs: can't mount /srv/people from 172.28.4.32 onto /Users/rutger/mnt: Operation not permitted
```

That is not a local permissions problem. The export carries `secure` (visible in §4), which
requires the client's source port to be below 1024, and macOS `mount_nfs` defaults to a
non-reserved port. The server refuses the mount and macOS reports it as `Operation not
permitted`.

```sh
sudo mount -t nfs -o resvport,vers=3 172.28.4.32:/srv/people ~/mnt
```

Fixed on the client deliberately. Adding `insecure` to the export would also work, but it
would weaken every export for every client to accommodate one, which is the wrong trade.

---

## 8 · The `nfsinfo` login banner

`/usr/local/bin/nfsinfo` prints service state, live exports, free space and the commands
worth remembering. `/etc/profile.d/nfs-status.sh` runs it on root SSH login.

```sh
cat > /usr/local/bin/nfsinfo <<'EOF'
#!/bin/sh
echo
echo "  NFS $(hostname)   nfs-server: $(systemctl is-active nfs-server)   versions: $(cat /proc/fs/nfsd/versions)"
echo
exportfs -v | sed 's/^/  /'
echo
df -h $(exportfs -s | awk '{print $1}' | sort -u) | awk 'NR==1 || !seen[$NF]++' | sed 's/^/  /'
echo
echo "  exportfs -ra    reload /etc/exports, no client disruption"
echo "  exportfs -v     show live exports"
echo "  showmount -a    who is mounted right now"
echo "  nfsinfo         this banner"
echo
EOF
chmod +x /usr/local/bin/nfsinfo
echo '[ -n "$PS1" ] && [ "$(id -u)" = 0 ] && nfsinfo' > /etc/profile.d/nfs-status.sh
```

`exportfs -s` prints the live export list in `exports(5)` format, so the `df` line follows
whatever is exported rather than a hardcoded path list. The `awk` deduplicates by mount
target, since all three exports currently share one filesystem. The `profile.d` hook fires for
root only, because `exportfs` needs root. `pct enter` gives a non-login shell, so type
`nfsinfo` there.

---

## 9 · Why it is built this way

**LXC, not a VM.** Without a web UI in the requirements there is no reason to give a guest its
own kernel. The container shares the host's, which means the host keeps the disk: SMART works,
LVM snapshots work, and the data is a bind mount rather than a passed-through block device.
The cost is a privileged container with AppArmor disabled, which is an acceptable trade on a
lab VLAN and would not be on anything exposed.

**Not TrueNAS or OpenMediaVault.** Both were considered. TrueNAS constrains exports to its own
form fields and has removed NFS over UDP entirely, which would have foreclosed the legacy
fallback. OMV would have worked, with a free-text export options field, but a UI was not
wanted and it brings its own `/export` pseudo-root indirection.

**Not FreeBSD.** Considered for legacy NFS support. Modern `nfsd(8)` serves v3 and v4 only,
with no NFSv2 in the man page, so it would not have solved the one problem it was being
considered for. Its only real advantage is UDP as a first-class flag rather than a deprecated
Linux toggle, and Linux `udp=y` works (§4).

**ext4, not XFS.** XFS defaults to `inode64` and can issue inode numbers above 2^32 on a
filesystem this size, which is the classic way to break old clients doing 32-bit `stat`.
ext4 numbers inodes sequentially and tops out around 130M on 2 TB, comfortably inside 32 bits.
*This is a precaution, not an observed failure* — all three SGIs are 6.5.30 and NFSv3 uses
64-bit fileids, so it may never have mattered. `mkfs.xfs` with `inode32` would have been
equally safe.

**No ZFS.** Rejected for RAM cost in the original VM design. Now that the host owns the disk
it could be reconsidered cheaply, since Proxmox already has ZFS loaded. The consequence of not
having it is that **backups are the only undo**, which raises the value of versioned offsite
copies in §11.

---

## 10 · Traps

1. **The disk was not empty.** `Usage: LVM` in the Proxmox disk list, and an orphaned
   `vm-102-disk-0` holding 1.82 TB. It kept the guest-shaped name long after the guest
   stopped referencing it. Always `pct config` the guest and grep every config before wiping.
2. **`cannot mount nfsd read-only`.** This is `/proc` mounted ro inside the container, not an
   AppArmor problem and not an nfsd problem. Fix is `lxc.mount.auto: proc:rw sys:rw`.
3. **Raw `lxc.*` keys need stop/start.** `pct reboot` re-runs the same failure.
4. **Unprivileged cannot be converted in place.** Rebuild, or `vzdump` and
   `pct restore --unprivileged 0`. Absence of an `unprivileged:` key means privileged;
   Proxmox only writes the key when it is 1.
5. **macOS fails with `Operation not permitted`** until you mount with `-o resvport`. It reads
   like a client-side permissions problem and is not. See §7.
6. **Debian GID 20 is `dialout`,** which collides if you try to recreate IRIX's `user` group
   by name. You do not need to (§5).
7. **The drive is SMR.** ST2000LM015 is shingled, confirmed in Seagate's own product manual:
   "Shingled magnetic recording with perpendicular magnetic recording heads/media", 5400 rpm,
   128 MB cache. Reads are normal and a nightly rsync over a mostly unchanged tree is fine.
   Sustained small-file writes collapse once the media cache fills, so **do not build on an
   NFS home backed by this disk** — keep build trees on local SGI disk. A separate `work`
   share on SSD was designed for this and then dropped; `/srv/cluster` now occupies that role
   but sits on the same spinning disk, so the warning stands.
8. **Bind mounts are excluded from `vzdump`.** Proxmox will back up container 105 and none of
   the data in `/srv`. The rsync job in §11 is the only thing protecting it.

---

## 11 · Still to do

- ☐ Power up an SGI and run the §6 test. Paste the `ls -ln` result into §4 and flip the
  heading on §6.
- ☐ Confirm lucy, arthur and aurora agree on rutger's UID.
- ☐ Seed `/usr/people` and cut over.
- ☐ Decide what `/srv/data` and `/srv/cluster` are actually for, and whether `data` keeps
  `no_root_squash`.
- ☐ **Backups.** Nightly push to the remote server over SSH:
  `rsync -aHAX --numeric-ids --delete --delete-delay`. `--numeric-ids` for the same reason as
  the seed. Prefer borg on the far side over a plain mirror, because `--delete` propagates
  mistakes within a day and there are no ZFS snapshots here to fall back on. The LVM snapshot
  reserve from §1 covers "about to do something stupid" locally, but it lives on the same disk
  and is not a backup.
- ☐ Decide whether `hd2` should become ZFS later (§9).
