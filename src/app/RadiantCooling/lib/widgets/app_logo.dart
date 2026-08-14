import 'package:flutter/material.dart';

/// The Radiant Cooling brand logo (rounded-square blue art with transparent
/// corners), sized and softly rounded to sit on any background.
class AppLogo extends StatelessWidget {
  const AppLogo({super.key, this.size = 96});

  final double size;

  @override
  Widget build(BuildContext context) {
    // Center + SizedBox keeps the logo at exactly `size` even when the
    // parent is a stretched column (e.g. the auth card), which would
    // otherwise force the logo as wide as the card.
    return Center(
      child: SizedBox(
        width: size,
        height: size,
        child: ClipRRect(
          borderRadius: BorderRadius.circular(size * 0.22),
          child: Image.asset(
            'assets/images/radiant-cooling-logo.png',
            width: size,
            height: size,
            fit: BoxFit.cover,
            // Decode at display size to keep memory low on small screens.
            cacheWidth: (size * MediaQuery.devicePixelRatioOf(context)).round(),
          ),
        ),
      ),
    );
  }
}
