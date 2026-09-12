// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
import ExpoModulesCore

public class SecureKeypadJsiModule: Module {
  public func definition() -> ModuleDefinition {
    Name("SecureKeypadJsi")

    // There is no `accessibility` prop on either platform. The keypad is kept
    // out of the accessibility tree unconditionally and is not screen-reader
    // usable by design. See README's screen-reader note.
    View(SecureKeypadJsiView.self) {
      Events("onDigitCountChanged", "onComplete", "onError")

      Prop("publicKey") { (view: SecureKeypadJsiView, pem: String) in view.setPublicKeyPem(pem) }
      Prop("keypadType") { (view: SecureKeypadJsiView, v: String) in view.setKeypadType(v) }
      Prop("minLength") { (view: SecureKeypadJsiView, v: Int) in view.setMinLength(v) }
      Prop("maxLength") { (view: SecureKeypadJsiView, v: Int) in view.setMaxLength(v) }
      Prop("shuffle") { (view: SecureKeypadJsiView, v: String) in view.setShuffle(v) }
      Prop("autoSubmit") { (view: SecureKeypadJsiView, v: Bool) in view.setAutoSubmit(v) }
      Prop("theme") { (view: SecureKeypadJsiView, v: [String: Any]) in view.setTheme(v) }

      AsyncFunction("clear") { (view: SecureKeypadJsiView) in view.clearPin() }
      AsyncFunction("submit") { (view: SecureKeypadJsiView) in view.submit() }
    }
  }
}
