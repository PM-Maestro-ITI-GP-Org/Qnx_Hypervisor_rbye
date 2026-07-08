#!/usr/bin/env bash
###############################################################################
#
# patch.sh
#
#   Applies QNX-specific patches to vsomeip source after download.sh.
#   Run this after download.sh and before build.sh:
#
#       bash scripts/download.sh
#       bash scripts/patch.sh
#       bash scripts/build.sh
#
###############################################################################
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
THIRD_PARTY="${ROOT_DIR}/third_party"
VSOMEIP_SRC="${THIRD_PARTY}/vsomeip"
OUTPUT_DIR="${OUTPUT_DIR:-${ROOT_DIR}/build-rpi}"

PATCH_FILE="${OUTPUT_DIR}/patches/vsomeip-qnx-pending-services.patch"

mkdir -p "$(dirname "${PATCH_FILE}")"

cat > "${PATCH_FILE}" <<'PATCHDIFF'
--- a/implementation/routing/src/routing_manager_impl.cpp
+++ b/implementation/routing/src/routing_manager_impl.cpp
@@ -4018,9 +4018,14 @@ void routing_manager_impl::on_net_interface_or_route_state_changed(

 void routing_manager_impl::start_ip_routing() {
 #if defined(_WIN32) || defined(__QNX__)
     if_state_running_ = true;
+    // On QNX/Windows there is no netlink connector to detect when the
+    // multicast route becomes available. Assume it is ready so that
+    // is_external_routing_ready() can return true when SD is enabled.
+    sd_route_set_ = true;
 #endif
 
     if (routing_ready_handler_) {
         routing_ready_handler_();
     }
@@ -4034,6 +4039,20 @@ void routing_manager_impl::start_ip_routing() {
 
     routing_running_ = true;
     VSOMEIP_INFO << VSOMEIP_ROUTING_READY_MESSAGE;
+
+#if defined(_WIN32) || defined(__QNX__)
+    // On QNX/Windows there is no netlink connector to call
+    // on_net_interface_or_route_state_changed() which would normally
+    // process pending service offers. Call init_pending_services() here
+    // so that services offered before routing was ready get their
+    // server endpoints created.
+    {
+        std::scoped_lock its_lock(on_state_change_mutex_);
+        if (is_external_routing_ready()) {
+            auto its_routing_state {get_routing_state()};
+            if (its_routing_state != routing_state_e::RS_SUSPENDED) {
+                init_pending_services();
+            }
+        }
+    }
+#endif
 }
 
 void routing_manager_impl::init_pending_services() {
PATCHDIFF

echo "============================================================"
echo "  Patching vsomeip for QNX..."
echo "============================================================"

cd "${VSOMEIP_SRC}"

# Check if already patched
if git apply --recount --check "${PATCH_FILE}" 2>/dev/null; then
    git apply --recount "${PATCH_FILE}"
    echo "  -> Patch applied successfully."
else
    echo "  -> Patch already applied or source modified — skipping."
fi

echo
echo "  Patch file saved to: ${PATCH_FILE}"
echo "============================================================"
