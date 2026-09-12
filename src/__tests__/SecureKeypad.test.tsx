import * as TestRenderer from 'react-test-renderer';

import { SecureKeypad } from '../SecureKeypad';

// requireNativeView returns a passthrough that records the props it was given.
const lastProps: { current: any } = { current: null };

// react-native ships untranspiled and this project's jest config has no
// transformIgnorePatterns for it, so stub the one API SecureKeypad calls.
jest.mock('react-native', () => ({
  StyleSheet: {
    flatten: (style: any) =>
      Array.isArray(style) ? Object.assign({}, ...style.filter(Boolean)) : style,
  },
  View: 'View',
  Text: 'Text',
}));

jest.mock('expo', () => {
  const RealReact = require('react');
  return {
    requireNativeView: () =>
      RealReact.forwardRef((props: any, _ref: any) => {
        lastProps.current = props;
        return null;
      }),
    requireNativeModule: () => ({}),
    NativeModule: class {},
  };
});

// The wrapper's only security-relevant job is the JS boundary: the public key
// reaches the native view, background wiping cannot be switched off, and the
// three native events are unwrapped so a ciphertext, a key-press count or an
// error never gets dropped on the floor. Theming and layout are exercised by
// the demos in example/app/, not here.
describe('SecureKeypad', () => {
  beforeEach(() => {
    lastProps.current = null;
  });

  it('passes the public key through and exposes no opt-out for background wiping', () => {
    TestRenderer.act(() => {
      TestRenderer.create(<SecureKeypad publicKey="PEM" maxLength={6} />);
    });
    expect(lastProps.current.publicKey).toBe('PEM');
    expect(lastProps.current.maxLength).toBe(6);
    expect(lastProps.current.clearOnBackground).toBeUndefined();
  });

  it('unwraps nativeEvent for onDigitCountChanged', () => {
    const onCount = jest.fn();
    TestRenderer.act(() => {
      TestRenderer.create(<SecureKeypad publicKey="PEM" onDigitCountChanged={onCount} />);
    });
    lastProps.current.onDigitCountChanged({ nativeEvent: { count: 3 } });
    expect(onCount).toHaveBeenCalledWith(3);
  });

  it('unwraps nativeEvent for onComplete', () => {
    const onComplete = jest.fn();
    TestRenderer.act(() => {
      TestRenderer.create(<SecureKeypad publicKey="PEM" onComplete={onComplete} />);
    });
    lastProps.current.onComplete({ nativeEvent: { envelope: '{"ct":"x"}' } });
    expect(onComplete).toHaveBeenCalledWith('{"ct":"x"}');
  });

  it('unwraps nativeEvent for onError', () => {
    const onError = jest.fn();
    TestRenderer.act(() => {
      TestRenderer.create(<SecureKeypad publicKey="PEM" onError={onError} />);
    });
    lastProps.current.onError({ nativeEvent: { code: 'ERR_WEAK_KEY', phase: 'arm' } });
    expect(onError).toHaveBeenCalledWith({ code: 'ERR_WEAK_KEY', phase: 'arm' });
  });
});
