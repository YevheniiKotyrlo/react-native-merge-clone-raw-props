#pragma once

#include <memory>
#include <string>

#include <ReactCommon/CallInvoker.h>
#include <ReactCommon/TurboModule.h>
#include <react/renderer/components/root/RootShadowNode.h>
#include <react/renderer/mounting/ShadowTree.h>
#include <react/renderer/uimanager/UIManagerBinding.h>
#include <react/renderer/uimanager/UIManagerCommitHook.h>

namespace reproducer {

// A commit hook that writes `opacity: 1` onto every node whose nativeID starts with a prefix, in every commit, passing
// only the props it writes to `cloneProps`, as Reanimated's ShadowTreeCloner does.
class PropsWritingCommitHook final : public facebook::react::UIManagerCommitHook {
 public:
  PropsWritingCommitHook(
      std::shared_ptr<facebook::react::UIManagerBinding> binding,
      std::string nativeIdPrefix);
  ~PropsWritingCommitHook() override;

  void commitHookWasRegistered(const facebook::react::UIManager& uiManager) noexcept override;
  void commitHookWasUnregistered(const facebook::react::UIManager& uiManager) noexcept override;

  facebook::react::RootShadowNode::Unshared shadowTreeWillCommit(
      const facebook::react::ShadowTree& shadowTree,
      const facebook::react::RootShadowNode::Shared& oldRootShadowNode,
      const facebook::react::RootShadowNode::Unshared& newRootShadowNode,
      const facebook::react::ShadowTreeCommitOptions& commitOptions) noexcept override;

 private:
  std::shared_ptr<facebook::react::ShadowNode> write(
      const facebook::react::ShadowNode& node,
      const facebook::react::PropsParserContext& context,
      facebook::react::ShadowTreeCommitSource source) const;

  const std::shared_ptr<facebook::react::UIManagerBinding> binding_;
  const std::string nativeIdPrefix_;
};

// `install(nativeIdPrefix)` registers the hook above.
class NativePropsWriter final : public facebook::react::TurboModule {
 public:
  static constexpr const char* kModuleName = "NativePropsWriter";

  explicit NativePropsWriter(std::shared_ptr<facebook::react::CallInvoker> jsInvoker);

 private:
  static facebook::jsi::Value install(
      facebook::jsi::Runtime& runtime,
      facebook::react::TurboModule& turboModule,
      const facebook::jsi::Value* arguments,
      size_t count);

  std::unique_ptr<PropsWritingCommitHook> hook_;
};

} // namespace reproducer
