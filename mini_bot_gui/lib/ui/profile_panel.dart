import 'package:flutter/material.dart';
import 'package:ocadi_bot_gui/app_constants/colors.dart' as mini_colors;
import 'package:ocadi_bot_gui/backend_services/drive_controller.dart';
import 'package:ocadi_bot_gui/models/drive_state.dart';

import 'section_heading.dart';

/// Profile selection panel: LIN / TRAP / SIN buttons (`PROFILE:...`),
/// highlighting the active profile from [DriveState.profile].
class ProfilePanel extends StatelessWidget {
  const ProfilePanel({
    super.key,
    required this.controller,
    required this.activeProfile,
    required this.enabled,
  });

  final DriveController controller;
  final DriveProfile activeProfile;
  final bool enabled;

  Widget _profileButton(
      String label, ProfileSelection selection, DriveProfile matches) {
    final isActive = activeProfile == matches;
    return ElevatedButton(
      onPressed: enabled ? () => controller.setProfile(selection) : null,
      style: ElevatedButton.styleFrom(
        backgroundColor:
            isActive ? mini_colors.lightRoyalPurpleHighlight : mini_colors.darkRoyalPurple,
        foregroundColor: mini_colors.offWhite,
        side: isActive
            ? const BorderSide(color: mini_colors.notQuiteBlack, width: 2)
            : null,
      ),
      child: Text(label),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16.0),
      child: Column(
        children: [
          const SectionHeading(title: 'Profile'),
          Wrap(
            alignment: WrapAlignment.center,
            spacing: 12,
            runSpacing: 8,
            children: [
              _profileButton('LIN', ProfileSelection.lin, DriveProfile.lin),
              _profileButton(
                  'TRAP', ProfileSelection.trap, DriveProfile.trap),
              _profileButton('SIN', ProfileSelection.sin, DriveProfile.sin),
            ],
          ),
          const SizedBox(height: 8),
        ],
      ),
    );
  }
}
