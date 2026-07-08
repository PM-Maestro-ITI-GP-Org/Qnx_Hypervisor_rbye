#!/usr/bin/env bash
###############################################################################
#
# download.sh
#
#   Clones / downloads every third-party dependency at the EXACT version
#   declared in env.sh, and force-checks-out the correct tag even if the
#   repository already exists (so re-running after a branch switch always
#   yields the right code).
#
#   Versions (all sourced from env.sh):
#
#     • vsomeip                    ${VSOMEIP_VERSION}                 (git tag)
#     • capicxx-core-runtime       ${COMMONAPI_CORE_RUNTIME_VERSION}  (git tag)
#     • capicxx-someip-runtime     ${COMMONAPI_SOMEIP_RUNTIME_VERSION}(git tag)
#     • boost                      ${BOOST_VERSION}                  (tarball)
#     • CommonAPI core generator   ${COMMONAPI_GENERATOR_VERSION}     (zip)
#     • CommonAPI someip generator ${COMMONAPI_GENERATOR_VERSION}     (zip)
#
###############################################################################

set -euo pipefail

# ── source env.sh for all version macros ──────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/env.sh"

THIRD_PARTY="${THIRD_PARTY_DIR}"
mkdir -p "${THIRD_PARTY}"

echo
echo "========================================="
echo " CommonAPI / vsomeip Download Script"
echo "========================================="
echo "  vsomeip               : ${VSOMEIP_VERSION}"
echo "  capicxx-core-runtime  : ${COMMONAPI_CORE_RUNTIME_VERSION}"
echo "  capicxx-someip-runtime: ${COMMONAPI_SOMEIP_RUNTIME_VERSION}"
echo "  generators            : ${COMMONAPI_GENERATOR_VERSION}"
echo "  boost                 : ${BOOST_VERSION}"
echo "========================================="
echo

# ── helper: clone OR checkout a git repo at a specific tag ────────────────
#   If the repo dir does not exist → clone at the tag.
#   If the repo dir already exists → fetch + checkout the tag (so a
#   previously-cloned master branch is switched to the correct version).
clone_repo()
{
    local NAME=$1
    local URL=$2
    local TAG=$3
    local DEST="${THIRD_PARTY}/${NAME}"

    echo "-----------------------------------------"
    echo "  ${NAME}  (tag ${TAG})"
    echo "-----------------------------------------"

    if [ ! -d "${DEST}" ]; then
        echo "  Cloning fresh …"
        git clone --depth 1 --branch "${TAG}" "${URL}" "${DEST}"
    else
        echo "  Repo exists — switching to tag ${TAG} …"
        (
            cd "${DEST}"
            git fetch --depth 1 origin "${TAG}" 2>/dev/null || git fetch origin
            git checkout "${TAG}"
        )
    fi
}

# ── helper: download a single file with curl ──────────────────────────────
download_file()
{
    local URL=$1
    local DEST=$2

    if [ -f "${DEST}" ]; then
        echo "[SKIP] $(basename "${DEST}") already exists."
        return
    fi

    echo "  Downloading $(basename "${DEST}")"
    curl -L --retry 3 -o "${DEST}" "${URL}"
}

###############################################################################
# 1. vsomeip  ${VSOMEIP_VERSION}
###############################################################################
clone_repo \
    vsomeip \
    https://github.com/COVESA/vsomeip.git \
    "${VSOMEIP_VERSION}"

###############################################################################
# 2. CommonAPI Core Runtime  ${COMMONAPI_CORE_RUNTIME_VERSION}
###############################################################################
clone_repo \
    capicxx-core-runtime \
    https://github.com/COVESA/capicxx-core-runtime.git \
    "${COMMONAPI_CORE_RUNTIME_VERSION}"

###############################################################################
# 3. CommonAPI SOME/IP Runtime  ${COMMONAPI_SOMEIP_RUNTIME_VERSION}
###############################################################################
clone_repo \
    capicxx-someip-runtime \
    https://github.com/COVESA/capicxx-someip-runtime.git \
    "${COMMONAPI_SOMEIP_RUNTIME_VERSION}"

###############################################################################
# 4. Boost  ${BOOST_VERSION}  (release tarball — self-contained)
###############################################################################
BOOST_TARBALL="${THIRD_PARTY}/${BOOST_DIRNAME}.tar.gz"
BOOST_URL="https://archives.boost.org/release/${BOOST_VERSION}/source/${BOOST_DIRNAME}.tar.gz"

echo "-----------------------------------------"
echo "  Boost ${BOOST_VERSION}  (tarball)"
echo "-----------------------------------------"

download_file "${BOOST_URL}" "${BOOST_TARBALL}"

if [ ! -d "${THIRD_PARTY}/${BOOST_DIRNAME}" ]; then
    echo "  Extracting ${BOOST_DIRNAME} …"
    tar xzf "${BOOST_TARBALL}" -C "${THIRD_PARTY}"
fi

###############################################################################
# 5. CommonAPI Core Generator  ${COMMONAPI_GENERATOR_VERSION}  (pre-built zip)
###############################################################################
CORE_GEN_ZIP="${THIRD_PARTY}/commonapi_core_generator.zip"
echo "-----------------------------------------"
echo "  CommonAPI Core Generator ${COMMONAPI_GENERATOR_VERSION}"
echo "-----------------------------------------"
download_file \
    "https://github.com/COVESA/capicxx-core-tools/releases/download/${COMMONAPI_GENERATOR_VERSION}/commonapi_core_generator.zip" \
    "${CORE_GEN_ZIP}"

###############################################################################
# 6. CommonAPI SOME/IP Generator  ${COMMONAPI_GENERATOR_VERSION}  (pre-built zip)
###############################################################################
SOMEIP_GEN_ZIP="${THIRD_PARTY}/commonapi_someip_generator.zip"
echo "-----------------------------------------"
echo "  CommonAPI SOME/IP Generator ${COMMONAPI_GENERATOR_VERSION}"
echo "-----------------------------------------"
download_file \
    "https://github.com/COVESA/capicxx-someip-tools/releases/download/${COMMONAPI_GENERATOR_VERSION}/commonapi_someip_generator.zip" \
    "${SOMEIP_GEN_ZIP}"

###############################################################################
# Done
###############################################################################
echo
echo "========================================="
echo " Download Complete"
echo "========================================="
echo
echo " Contents of ${THIRD_PARTY}:"
find "${THIRD_PARTY}" -maxdepth 1 -mindepth 1 -printf "  - %f\n" | sort
echo
