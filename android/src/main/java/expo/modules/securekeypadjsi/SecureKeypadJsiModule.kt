// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
package expo.modules.securekeypadjsi

import expo.modules.kotlin.modules.Module
import expo.modules.kotlin.modules.ModuleDefinition

class SecureKeypadJsiModule : Module() {
  override fun definition() = ModuleDefinition {
    Name("SecureKeypadJsi")

    // Runs on the UI thread when the host activity pauses. Views wipe their
    // buffers here; iOS does the same from
    // UIApplication.didEnterBackgroundNotification.
    OnActivityEntersBackground {
      SecureKeypadJsiView.onActivityEntersBackground()
    }

    View(SecureKeypadJsiView::class) {
      Events("onDigitCountChanged", "onComplete", "onError")

      Prop("publicKey") { view: SecureKeypadJsiView, pem: String -> view.setPublicKeyPem(pem) }
      Prop("keypadType") { view: SecureKeypadJsiView, v: String -> view.setKeypadType(v) }
      Prop("minLength") { view: SecureKeypadJsiView, v: Int -> view.setMinLength(v) }
      Prop("maxLength") { view: SecureKeypadJsiView, v: Int -> view.setMaxLength(v) }
      Prop("shuffle") { view: SecureKeypadJsiView, v: String -> view.setShuffle(v) }
      Prop("autoSubmit") { view: SecureKeypadJsiView, v: Boolean -> view.setAutoSubmit(v) }
      Prop("theme") { view: SecureKeypadJsiView, v: Map<String, Any?> -> view.setTheme(v) }

      AsyncFunction("clear") { view: SecureKeypadJsiView -> view.clearPin() }
      AsyncFunction("submit") { view: SecureKeypadJsiView -> view.submit() }

      OnViewDestroys { view: SecureKeypadJsiView -> view.dispose() }
    }
  }
}
