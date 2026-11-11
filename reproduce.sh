#!/usr/bin/env bash
# Builds ReproducerApp's release APK with enableFabricCommitBranching on, installs and launches it on the device adb
# reaches (ANDROID_SERIAL picks one), and prints the clones the commit hook logs under the PropsWriter tag.
# Exit 1 while the hook's clone in the merge of React's revision loses the raw props React set, 0 once it keeps them,
# 2 when a step fails. Gradle properties given as arguments reach the build: ./reproduce.sh -PcommitBranching=false
set -u

here="$(cd "$(dirname "$0")" && pwd)"
app="${here}/ReproducerApp"
application_id=com.reproducerapp

system="$(uname -sm)"
if ! node_version="$(node --version 2>&1)"; then
    echo "step failed: node --version: ${node_version}"
    exit 2
fi
if ! java_version="$(java -version 2>&1)"; then
    echo "step failed: java -version: ${java_version}"
    exit 2
fi
if ! device="$(adb shell getprop ro.product.model 2>&1)" || ! android_version="$(adb shell getprop ro.build.version.release 2>&1)"; then
    echo "step failed: reading the device: ${device}"
    exit 2
fi
echo "${system}, node ${node_version}, ${java_version%%$'\n'*}, device ${device//$'\r'/} on Android ${android_version//$'\r'/}"

if ! install_output="$(cd "${app}" && yarn install --frozen-lockfile --non-interactive 2>&1)"; then
    printf '%s\n' "${install_output}"
    echo "step failed: yarn install"
    exit 2
fi
if ! react_native_version="$(cd "${app}" && node -p 'require("react-native/package.json").version' 2>&1)"; then
    echo "step failed: reading react-native's version: ${react_native_version}"
    exit 2
fi
echo "react-native ${react_native_version}"

echo "== ./gradlew assembleRelease -PreactNativeArchitectures=arm64-v8a $*"
if ! build_output="$(cd "${app}/android" && ./gradlew --no-daemon --console=plain assembleRelease -PreactNativeArchitectures=arm64-v8a "$@" 2>&1)"; then
    printf '%s\n' "${build_output}" | tail -n 40
    echo "SUMMARY: the release build failed"
    exit 2
fi
grep -E "^BUILD SUCCESSFUL" <<<"${build_output}"

echo "== adb install, then launch ${application_id}"
if ! install_output="$(cd "${app}/android" && adb install -r app/build/outputs/apk/release/app-release.apk 2>&1)"; then
    printf '%s\n' "${install_output}"
    echo "SUMMARY: adb install failed"
    exit 2
fi
adb shell am force-stop "${application_id}"
adb logcat -c
if ! launch_output="$(adb shell am start -W -n "${application_id}/.MainActivity" 2>&1)"; then
    printf '%s\n' "${launch_output}"
    echo "SUMMARY: launching the app failed"
    exit 2
fi

# The merge of React's revision runs in a later frame; its clone is the line the verdict reads.
timeout 60 adb logcat -v tag -s PropsWriter:I | grep -m 1 -q "commit=ReactRevisionMerge" || true
if ! logged="$(adb logcat -d -v tag -s PropsWriter:I 2>&1)"; then
    printf '%s\n' "${logged}"
    echo "SUMMARY: reading logcat failed"
    exit 2
fi
clones="$(sed -E 's#^I/PropsWriter *: #PropsWriter: #; s#\r$##' <<<"${logged}")"
echo "== adb logcat -s PropsWriter"
printf '%s\n' "${clones}"
adb exec-out screencap -p > screenshot.png && echo "screenshot: screenshot.png"
# testID reaches the view as its resource-id.
if adb shell uiautomator dump /sdcard/reproducer-window.xml > /dev/null && window="$(adb shell cat /sdcard/reproducer-window.xml)"; then
    box_view="$(grep -o 'resource-id="written-box"[^>]*bounds="[^"]*"' <<<"${window}")"
    if [[ -n "${box_view}" ]]; then
        echo "the view with resource-id written-box: bounds ${box_view##*bounds=}"
    else
        echo "the view with resource-id written-box: none on screen"
    fi
fi

merge_clone="$(grep -m 1 "commit=ReactRevisionMerge" <<<"${clones}")"
if [[ -z "${merge_clone}" ]]; then
    echo "SUMMARY: the hook logged no clone in a merge of React's revision (is enableFabricCommitBranching on?)"
    exit 2
fi
if [[ "${merge_clone}" == *"source rawProps: nativeID=yes backgroundColor=yes; written rawProps: nativeID=no backgroundColor=no"* ]]; then
    echo "SUMMARY: the commit hook's clone in the merge of React's revision (ReactRevisionMerge) holds neither the nativeID nor the backgroundColor React set"
    exit 1
fi
if [[ "${merge_clone}" == *"written rawProps: nativeID=yes backgroundColor=yes"* ]]; then
    echo "SUMMARY: the hook's clone in the merge keeps the raw props React set"
    exit 0
fi
echo "SUMMARY: the merge's clone logged something else"
exit 2
