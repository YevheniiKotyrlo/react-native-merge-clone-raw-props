# Android: under `enableFabricCommitBranching`, a commit hook's clone in the branch's merge drops the props the view is created from

A reproducer on React Native's nightly of `main` (`0.89.0-nightly-20261030-4afc5d5b9`), Android only.

`ReproducerApp/android/app/src/main/jni/NativePropsWriter.cpp` is a C++ TurboModule whose `install(prefix)` registers a `UIManagerCommitHook`. In every commit, the hook writes `opacity: 1` onto each node whose `nativeID` starts with the prefix, passing only that prop to `cloneProps`, as Reanimated's `ShadowTreeCloner` does. `App.tsx` installs it for `written-`, then renders a red 200 × 200 box with `nativeID` and `testID` `written-box`. `MainApplication.kt` turns on `enableFabricCommitBranching`.

## Steps

1. `cd ReproducerApp && yarn install`
2. `cd android && ./gradlew assembleRelease -PreactNativeArchitectures=arm64-v8a`
3. `adb install -r app/build/outputs/apk/release/app-release.apk`, launch the app, and read `adb logcat -s PropsWriter`.
4. Repeat steps 2 and 3 with `-PcommitBranching=false`.

**Expected** (as with `-PcommitBranching=false`): the red box shows, and the view has its `nativeID` and `testID`.

**Actual** with the flag on: no box shows. The hook's clone in React's commit keeps the node's raw props, but its clone in the merge (`commit=ReactRevisionMerge`) keeps only the one it was given, `opacity`. The merge creates the view from those raw props, without its background colour, `nativeID` or `testID`:

```text
PropsWriter: commit=React tag=10 source rawProps: nativeID=yes backgroundColor=yes; written rawProps: nativeID=yes backgroundColor=yes
PropsWriter: commit=ReactRevisionMerge tag=10 source rawProps: nativeID=yes backgroundColor=yes; written rawProps: nativeID=no backgroundColor=no
```

With the flag off, the view is created in React's own commit, from the clone that kept the node's raw props.
