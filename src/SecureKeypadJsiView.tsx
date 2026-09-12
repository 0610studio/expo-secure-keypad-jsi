import { requireNativeView } from 'expo';
import * as React from 'react';

import type { SecureKeypadHandle, SecureKeypadJsiViewProps } from './SecureKeypadJsi.types';

const NativeView: React.ComponentType<
  SecureKeypadJsiViewProps & { ref?: React.Ref<SecureKeypadHandle> }
> = requireNativeView('SecureKeypadJsi');

/** Most apps should use `SecureKeypad` instead. */
const SecureKeypadJsiView = React.forwardRef<SecureKeypadHandle, SecureKeypadJsiViewProps>(
  (props, ref) => {
    return <NativeView {...props} ref={ref} />;
  }
);

SecureKeypadJsiView.displayName = 'SecureKeypadJsiView';

export default SecureKeypadJsiView;
