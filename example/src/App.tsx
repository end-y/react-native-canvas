// API test gallery: each page exercises one area of the ctx API with a small
// scenario (DESIGN §4). Navigation is plain state — no nav dependency.
import { useState } from 'react';
import {
  View,
  Text,
  Pressable,
  ScrollView,
  StyleSheet,
  SafeAreaView,
} from 'react-native';

import BubblesPage from './pages/BubblesPage';
import ShapesPage from './pages/ShapesPage';
import LineStylesPage from './pages/LineStylesPage';
import TransformsPage from './pages/TransformsPage';
import GradientsPage from './pages/GradientsPage';
import ShadowsPage from './pages/ShadowsPage';
import CompositePage from './pages/CompositePage';
import ClipPage from './pages/ClipPage';
import HitTestPage from './pages/HitTestPage';

const PAGES = [
  {
    key: 'bubbles',
    title: 'Bubbles — perf & onPress',
    subtitle: 'fillInstances · Path2D · 15k load · manual hit-testing',
    Component: BubblesPage,
  },
  {
    key: 'shapes',
    title: 'Shapes & Curves',
    subtitle: 'bezier · quadratic · arcTo · ellipse · roundRect · arc',
    Component: ShapesPage,
  },
  {
    key: 'linestyles',
    title: 'Line Styles',
    subtitle: 'lineCap · lineJoin · miterLimit',
    Component: LineStylesPage,
  },
  {
    key: 'transforms',
    title: 'Transforms',
    subtitle: 'translate · rotate · scale · save/restore · setTransform',
    Component: TransformsPage,
  },
  {
    key: 'gradients',
    title: 'Gradients',
    subtitle: 'createLinearGradient · createRadialGradient · gradient stroke',
    Component: GradientsPage,
  },
  {
    key: 'shadows',
    title: 'Shadows',
    subtitle: 'shadowColor · shadowBlur · shadowOffsetX/Y',
    Component: ShadowsPage,
  },
  {
    key: 'composite',
    title: 'Composite Ops',
    subtitle: 'globalCompositeOperation — all 26 modes',
    Component: CompositePage,
  },
  {
    key: 'clip',
    title: 'Clip & Fill Rules',
    subtitle: "clip() · fill('evenodd') · save/restore",
    Component: ClipPage,
  },
  {
    key: 'hittest',
    title: 'Hit Testing',
    subtitle: 'isPointInPath · isPointInStroke · Path2D overloads',
    Component: HitTestPage,
  },
];

export default function App() {
  const [pageKey, setPageKey] = useState<string | null>(null);
  const page = PAGES.find((p) => p.key === pageKey);

  if (page) {
    const PageComponent = page.Component;
    return (
      <View style={styles.root}>
        <PageComponent />
        <Pressable style={styles.back} onPress={() => setPageKey(null)}>
          <Text style={styles.backText}>‹ Menu</Text>
        </Pressable>
      </View>
    );
  }

  return (
    <SafeAreaView style={styles.root}>
      <ScrollView contentContainerStyle={styles.menu}>
        <Text style={styles.header}>react-native-canvas</Text>
        <Text style={styles.subheader}>API test gallery</Text>
        {PAGES.map((p) => (
          <Pressable
            key={p.key}
            style={styles.item}
            onPress={() => setPageKey(p.key)}
          >
            <Text style={styles.itemTitle}>{p.title}</Text>
            <Text style={styles.itemSubtitle}>{p.subtitle}</Text>
          </Pressable>
        ))}
      </ScrollView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  root: { flex: 1, backgroundColor: '#11131a' },
  menu: { padding: 20, paddingTop: 24 },
  header: { color: '#fff', fontSize: 26, fontWeight: '700' },
  subheader: { color: '#8a8f9e', fontSize: 15, marginBottom: 18 },
  item: {
    backgroundColor: 'rgba(255,255,255,0.07)',
    borderRadius: 12,
    padding: 16,
    marginBottom: 10,
  },
  itemTitle: { color: '#fff', fontSize: 17, fontWeight: '600' },
  itemSubtitle: { color: '#8a8f9e', fontSize: 13, marginTop: 3 },
  back: {
    position: 'absolute',
    top: 56,
    left: 16,
    backgroundColor: 'rgba(0,0,0,0.55)',
    paddingHorizontal: 14,
    paddingVertical: 8,
    borderRadius: 8,
  },
  backText: { color: '#fff', fontSize: 15, fontWeight: '600' },
});
