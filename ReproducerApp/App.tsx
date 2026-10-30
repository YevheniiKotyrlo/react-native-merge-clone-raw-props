/**
 * A red box, created after a commit hook that writes `opacity: 1` onto it in every commit is installed.
 *
 * @format
 */

import { useEffect, useState } from 'react';
import type { TurboModule } from 'react-native';
import { StyleSheet, Text, TurboModuleRegistry, View } from 'react-native';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';

interface PropsWriter extends TurboModule {
  install(nativeIdPrefix: string): void;
}

const propsWriter = TurboModuleRegistry.getEnforcing<PropsWriter>('NativePropsWriter');

function App() {
  const [boxShown, setBoxShown] = useState(false);

  useEffect(() => {
    propsWriter.install('written-');
    setBoxShown(true);
  }, []);

  return (
    <SafeAreaProvider>
      <SafeAreaView style={styles.container}>
        <Text>Below this line, a red 200 × 200 box with nativeID and testID "written-box":</Text>
        {boxShown ? <View nativeID="written-box" testID="written-box" style={styles.box} /> : null}
      </SafeAreaView>
    </SafeAreaProvider>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    padding: 24,
  },
  box: {
    width: 200,
    height: 200,
    marginTop: 16,
    backgroundColor: 'red',
  },
});

export default App;
