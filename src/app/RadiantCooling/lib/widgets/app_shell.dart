import 'package:flutter/material.dart';

import 'app_logo.dart';

/// The app's main shell chrome: AppBar with the brand logo, an
/// [IndexedStack] body, and the bottom [NavigationBar]. Used by [HomeShell]
/// (lib/main.dart) and by the screenshot harness so captures always match
/// the real UI.
class AppShell extends StatelessWidget {
  const AppShell({
    super.key,
    required this.tabIndex,
    required this.onTabChanged,
    required this.children,
  });

  final int tabIndex;
  final ValueChanged<int> onTabChanged;
  final List<Widget> children;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Radiant Cooling'),
        leading: Padding(
          padding: const EdgeInsets.only(left: 12),
          child: Center(child: AppLogo(size: 32)),
        ),
        leadingWidth: 56,
      ),
      body: IndexedStack(index: tabIndex, children: children),
      bottomNavigationBar: NavigationBar(
        selectedIndex: tabIndex,
        onDestinationSelected: onTabChanged,
        destinations: const [
          NavigationDestination(
            icon: Icon(Icons.dashboard_outlined),
            selectedIcon: Icon(Icons.dashboard),
            label: 'Dashboard',
          ),
          NavigationDestination(
            icon: Icon(Icons.settings_outlined),
            selectedIcon: Icon(Icons.settings),
            label: 'Settings',
          ),
        ],
      ),
    );
  }
}
