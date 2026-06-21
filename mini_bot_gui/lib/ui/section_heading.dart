import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';
import 'package:ocadi_bot_gui/app_constants/colors.dart' as mini_colors;

/// A section heading using the app's lexend heading style, followed by
/// the standard divider. Matches the visual style of the original app.
class SectionHeading extends StatelessWidget {
  const SectionHeading({super.key, required this.title});

  final String title;

  @override
  Widget build(BuildContext context) {
    return Column(
      children: [
        Padding(
          padding: const EdgeInsets.only(top: 16.0, bottom: 4.0),
          child: Text(
            title,
            textAlign: TextAlign.center,
            style: GoogleFonts.lexend(
              textStyle: const TextStyle(
                color: mini_colors.notQuiteBlack,
                fontSize: 24,
                fontWeight: FontWeight.w600,
              ),
            ),
          ),
        ),
        const Divider(
          height: 5,
          thickness: 2,
          indent: 42,
          endIndent: 42,
          color: mini_colors.darkRoyalPurpleHighlight,
        ),
      ],
    );
  }
}
