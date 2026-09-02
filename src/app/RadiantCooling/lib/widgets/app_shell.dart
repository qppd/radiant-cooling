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
    this.alertBadgeCount = 0,
  });

  final int tabIndex;
  final ValueChanged<int> onTabChanged;
  final List<Widget> children;

  /// Number to show as a badge on the Alerts tab icon. Zero hides the badge.
  final int alertBadgeCount;

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
        destinations: [
          const NavigationDestination(
            icon: Icon(Icons.dashboard_outlined),
            selectedIcon: Icon(Icons.dashboard),
            label: 'Dashboard',
          ),
          const NavigationDestination(
            icon: Icon(Icons.show_chart_outlined),
            selectedIcon: Icon(Icons.show_chart),
            label: 'Trends',
          ),
          NavigationDestination(
            icon: Badge(
              label: alertBadgeCount > 0 ? Text('$alertBadgeCount') : null,
              isLabelVisible: alertBadgeCount > 0,
              child: const Icon(Icons.notifications_outlined),
            ),
            selectedIcon: Badge(
              label: alertBadgeCount > 0 ? Text('$alertBadgeCount') : null,
              isLabelVisible: alertBadgeCount > 0,
              child: const Icon(Icons.notifications),
            ),
            label: 'Alerts',
          ),
          const NavigationDestination(
            icon: Icon(Icons.settings_outlined),
            selectedIcon: Icon(Icons.settings),
            label: 'Settings',
          ),
        ],
      ),
    );
  }
}
