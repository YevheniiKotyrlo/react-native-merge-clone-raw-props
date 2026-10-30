#include "NativePropsWriter.h"

#include <android/log.h>

#include <utility>
#include <vector>

#include <folly/dynamic.h>
#include <react/renderer/core/PropsParserContext.h>
#include <react/renderer/core/RawProps.h>

using namespace facebook;
using namespace facebook::react;

namespace reproducer {

namespace {

const char* sourceName(ShadowTreeCommitSource source) {
  switch (source) {
    case ShadowTreeCommitSource::Unknown:
      return "Unknown";
    case ShadowTreeCommitSource::React:
      return "React";
    case ShadowTreeCommitSource::AnimationEndSync:
      return "AnimationEndSync";
    case ShadowTreeCommitSource::ReactRevisionMerge:
      return "ReactRevisionMerge";
  }
  return "Unknown";
}

const char* holds(const Props& props, const char* name) {
  return props.rawProps.count(name) > 0 ? "yes" : "no";
}

} // namespace

PropsWritingCommitHook::PropsWritingCommitHook(std::shared_ptr<UIManagerBinding> binding, std::string nativeIdPrefix)
    : binding_(std::move(binding)), nativeIdPrefix_(std::move(nativeIdPrefix)) {
  binding_->getUIManager().registerCommitHook(*this);
}

PropsWritingCommitHook::~PropsWritingCommitHook() {
  binding_->getUIManager().unregisterCommitHook(*this);
}

void PropsWritingCommitHook::commitHookWasRegistered(const UIManager& /*uiManager*/) noexcept {}

void PropsWritingCommitHook::commitHookWasUnregistered(const UIManager& /*uiManager*/) noexcept {}

std::shared_ptr<ShadowNode> PropsWritingCommitHook::write(
    const ShadowNode& node,
    const PropsParserContext& context,
    ShadowTreeCommitSource source) const {
  const auto& children = node.getChildren();
  std::vector<std::shared_ptr<const ShadowNode>> writtenChildren;
  for (size_t index = 0; index < children.size(); ++index) {
    if (auto writtenChild = write(*children[index], context, source)) {
      if (writtenChildren.empty()) {
        writtenChildren = children;
      }
      writtenChildren[index] = std::move(writtenChild);
    }
  }
  const bool matches = node.getProps()->nativeId.starts_with(nativeIdPrefix_);
  if (!matches && writtenChildren.empty()) {
    return nullptr;
  }
  Props::Shared props = nullptr;
  if (matches) {
    props = node.getComponentDescriptor().cloneProps(
        context, node.getProps(), RawProps(folly::dynamic::object("opacity", 1)));
  }
  auto written = node.clone(
      {.props = props,
       .children = writtenChildren.empty()
           ? nullptr
           : std::make_shared<const std::vector<std::shared_ptr<const ShadowNode>>>(std::move(writtenChildren)),
       .state = ShadowNodeFragment::statePlaceholder()});
  if (matches) {
    __android_log_print(
        ANDROID_LOG_INFO,
        "PropsWriter",
        "commit=%s tag=%d source rawProps: nativeID=%s backgroundColor=%s; written rawProps: nativeID=%s "
        "backgroundColor=%s",
        sourceName(source),
        node.getTag(),
        holds(*node.getProps(), "nativeID"),
        holds(*node.getProps(), "backgroundColor"),
        holds(*written->getProps(), "nativeID"),
        holds(*written->getProps(), "backgroundColor"));
  }
  return written;
}

RootShadowNode::Unshared PropsWritingCommitHook::shadowTreeWillCommit(
    const ShadowTree& shadowTree,
    const RootShadowNode::Shared& /*oldRootShadowNode*/,
    const RootShadowNode::Unshared& newRootShadowNode,
    const ShadowTreeCommitOptions& commitOptions) noexcept {
  const PropsParserContext context{shadowTree.getSurfaceId(), *binding_->getUIManager().getContextContainer()};
  if (auto written = write(*newRootShadowNode, context, commitOptions.source)) {
    return std::static_pointer_cast<RootShadowNode>(std::move(written));
  }
  return newRootShadowNode;
}

NativePropsWriter::NativePropsWriter(std::shared_ptr<CallInvoker> jsInvoker)
    : TurboModule(kModuleName, std::move(jsInvoker)) {
  methodMap_["install"] = MethodMetadata{.argCount = 1, .invoker = install};
}

jsi::Value NativePropsWriter::install(
    jsi::Runtime& runtime,
    TurboModule& turboModule,
    const jsi::Value* arguments,
    size_t count) {
  if (count != 1 || !arguments[0].isString()) {
    throw jsi::JSError(runtime, "install takes a nativeID prefix");
  }
  auto& module = static_cast<NativePropsWriter&>(turboModule);
  if (module.hook_ == nullptr) {
    module.hook_ = std::make_unique<PropsWritingCommitHook>(
        UIManagerBinding::getBinding(runtime), arguments[0].asString(runtime).utf8(runtime));
  }
  return jsi::Value::undefined();
}

} // namespace reproducer
