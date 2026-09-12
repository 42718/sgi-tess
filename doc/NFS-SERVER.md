# NFS home server — shared `/usr/people` for lucy, arthur and aurora

Server built and verified on `ha` (Proxmox), **12 September 2026**. Every output in §4 was
pasted from the machine. The client side in §5 is written but **not yet cut over**, and says so.

Replaces the UniFi UNAS 2-bay, which forces `root_squash` on NFS with no per-export control.
Root-owned files arrive as `nobody`, which makes it unusable for `/usr/people` homes and for
anything the cluster does as root.

## 0 · What was built

| | |
|---|---|
| Host | `ha`, Intel N100 NUC, Proxmox VE 8.4.12 |
| Container | LXC **105**, hostname `nfs`, Debian 13, **privileged** |
| Disk | `/dev/sdb`, Seagate ST2000LM015, 2 TB, 5400 rpm |
| Volume | VG `hd2`, LV `nfsstore`, ext4, mounted at `/srv` on the host |
| Export | `/srv/people`, NFSv3 only, AUTH_SYS, `no_root_squash` |
| Clients | lucy `172.28.4.8`, arthur `172.28.4.17`, aurora `172.28.4.16`, all IRIX 6.5.30 |
| Container IP | `172.28.4.32` |

---

## 1 · Storage

> The LVM commands below are as prescribed during the build. `lvs` output was not captured,
> so confirm the LV name and size on `ha` before trusting the figures in this section.

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

`/etc/fstab` on `ha`:

```
/dev/hd2/nfsstore  /srv  ext4  defaults,noatime  0 2
```

```sh
mount /srv
mkdir -p /srv/people
umount /srv
chattr +i /srv
mount /srv
```

The `chattr +i` is deliberate. If `/srv` ever fails to mount, the container would otherwise
bind the empty directory underneath it and write homes onto the boot SSD, silently, until it
filled. Immutable-while-unmounted makes that failure impossible instead of invisible.

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
1:1, so the ownership the SGIs write is the ownership on disk. Unprivileged shifts everything
by 100000 and the IRIX UIDs land somewhere useless.

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
/srv/people  172.28.4.0/24(rw,sync,no_root_squash,no_subtree_check,sec=sys)
```

```sh
systemctl enable --now nfs-server
exportfs -ra
```

---

## 4 · Verification, as observed

```
# exportfs -v
/srv/people     172.28.4.0/24(sync,wdelay,hide,no_subtree_check,sec=sys,rw,secure,no_root_squash,no_all_squash)

# cat /proc/fs/nfsd/versions
+3 -4 -4.0 -4.1 -4.2

# rpcinfo -p localhost | grep nfs
    100003    3   tcp   2049  nfs
    100227    3   tcp   2049  nfs_acl
    100003    3   udp   2049  nfs
    100227    3   udp   2049  nfs_acl
```

☐ `no_root_squash` present in the `exportfs -v` option list.
☐ `+3` and every v4 variant negative.
☐ Program 100003 version 3 on **both** udp and tcp.

Note what is *absent*: NFSv2 is not listed at all. Debian ships `CONFIG_NFSD_V2` disabled, and
it is deprecated upstream. All three clients are 6.5.30 and speak v3, so this costs nothing
here, but a pre-6.5 machine could not use this server and would need a FreeBSD host instead.

---

## 5 · IRIX clients — NOT YET DONE

> **Pasting these blocks into IRIX:** the root shell is `csh`, which does not honour `#`
> comments interactively — it globs what follows and aborts on an unmatched `?` or `*`.
> Type `sh` first and paste into that, or paste one command at a time.

### Identity: no accounts are needed on the server

With v4 off, AUTH_SYS carries numeric UIDs and GIDs over the wire and the server never
consults a name. So the server needs **no** `rutger` account, no matching group, nothing.
Ownership is set with raw numbers.

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

**All three hosts must agree on the UID.** Check `grep rutger /etc/passwd` on lucy, arthur and
aurora. A home shared between hosts that disagree looks owned by a stranger on one of them.
Fix the odd host out with `usermod -u` and re-chown its local files *before* migrating.

If you want `ls -l` on the Debian side to show names rather than bare numbers, note that
GID 20 is `dialout` on Debian, so a plain `groupadd -g 20 user` fails. Use
`groupadd -o -g <gid> sgiuser` to allow the duplicate. Purely cosmetic; it changes nothing
about how NFS behaves.

### Test before migrating anything

```sh
showmount -e nfs
mkdir -p /mnt/test
mount -o vers=3,proto=tcp nfs:/srv/people /mnt/test
touch /mnt/test/roottest
ls -ln /mnt/test/roottest
```

☐ `ls -ln` reports uid 0, gid 0. If it says 60001 or `nobody`, root is still being squashed
and nothing below should proceed.
☐ `showmount` answers rather than hanging. A hang points at the container's Proxmox firewall.

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

## 6 · Why it is built this way

**LXC, not a VM.** Without a web UI in the requirements there is no reason to give a guest its
own kernel. The container shares the host's, which means the host keeps the disk: SMART works,
LVM snapshots work, and the data is a bind mount rather than a passed-through block device.
The cost is a privileged container with AppArmor disabled, which is an acceptable trade on a
lab VLAN and would not be on anything exposed.

**Not FreeBSD.** It was considered for legacy NFS support. Modern `nfsd(8)` serves v3 and v4
only, with no NFSv2 in the man page, so it would not have solved the one problem it was
being considered for. Its only real advantage is UDP as a first-class flag rather than a
deprecated Linux toggle, and Linux `udp=y` works (§4).

**ext4, not XFS.** XFS defaults to `inode64` and can issue inode numbers above 2^32 on a
filesystem this size, which is the classic way to break old clients doing 32-bit `stat`.
ext4 numbers inodes sequentially and tops out around 130M on 2 TB, comfortably inside 32 bits.
*This is a precaution, not an observed failure* — all three clients are 6.5.30 and NFSv3 uses
64-bit fileids, so it may never have mattered. `mkfs.xfs` with `inode32` would have been
equally safe.

**No ZFS.** Rejected for RAM cost in the original VM design. Now that the host owns the disk
it could be reconsidered cheaply, since Proxmox already has ZFS loaded. The consequence of not
having it is that **backups are the only undo**, which raises the value of versioned offsite
copies in §8.

---

## 7 · Traps

1. **The disk was not empty.** `Usage: LVM` in the Proxmox disk list, and an orphaned
   `vm-102-disk-0` holding 1.82 TB. It kept the guest-shaped name long after the guest
   stopped referencing it. Always `pct config` the guest and grep every config before wiping.
2. **`cannot mount nfsd read-only`.** This is `/proc` mounted ro inside the container, not an
   AppArmor problem and not an nfsd problem. Fix is `lxc.mount.auto: proc:rw sys:rw`.
3. **Raw `lxc.*` keys need stop/start.** `pct reboot` re-runs the same failure.
4. **Unprivileged cannot be converted in place.** Rebuild, or `vzdump` and
   `pct restore --unprivileged 0`. Absence of an `unprivileged:` key means privileged;
   Proxmox only writes the key when it is 1.
5. **Debian GID 20 is `dialout`,** which collides if you try to recreate IRIX's `user` group
   by name. You do not need to (§5).
6. **The drive is SMR.** ST2000LM015 is shingled, confirmed in Seagate's own product manual:
   "Shingled magnetic recording with perpendicular magnetic recording heads/media", 5400 rpm,
   128 MB cache. Reads are normal and a nightly rsync over a mostly unchanged tree is fine.
   Sustained small-file writes collapse once the media cache fills, so **do not build on an
   NFS home backed by this disk** — keep build trees on local SGI disk. A separate `work`
   share on SSD was designed for this and then dropped; if compiling over NFS ever becomes
   necessary, that is the thing to revive.
7. **Bind mounts are excluded from `vzdump`.** Proxmox will back up container 105 and none of
   the data in `/srv`. The rsync job in §8 is the only thing protecting it.

---

## 8 · Still to do

- ☐ Run the §5 client test on one SGI and paste the `ls -ln` result into §4.
- ☐ Confirm lucy, arthur and aurora agree on rutger's UID.
- ☐ Seed `/usr/people` and cut over.
- ☐ **Backups.** Nightly push to the remote server over SSH:
  `rsync -aHAX --numeric-ids --delete --delete-delay`. `--numeric-ids` for the same reason as
  the seed. Prefer borg on the far side over a plain mirror, because `--delete` propagates
  mistakes within a day and there are no ZFS snapshots here to fall back on. The LVM snapshot
  reserve from §1 covers "about to do something stupid" locally, but it lives on the same disk
  and is not a backup.
- ☐ Decide whether `hd2` should become ZFS later (§6).
