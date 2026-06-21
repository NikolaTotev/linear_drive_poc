import 'package:flutter/material.dart';
import 'package:ocadi_bot_gui/app_constants/colors.dart' as mini_colors;
import 'package:ocadi_bot_gui/backend_services/drive_controller.dart';
import 'package:ocadi_bot_gui/models/drive_state.dart';

import 'section_heading.dart';

/// Calibration panel: Calibrate (`CAL`) and Home (`HOME`) buttons, plus the
/// last calibration result (rangePulses) when available.
class CalibrationPanel extends StatelessWidget {
  const CalibrationPanel({
    super.key,
    required this.controller,
    required this.state,
    required this.enabled,
  });

  final DriveController controller;
  final DriveState state;

  /// Whether commands may be sent (e.g. false while e-stop is latched).
  final bool enabled;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16.0),
      child: Column(
        children: [
          const SectionHeading(title: 'Calibration'),
          Wrap(
            alignment: WrapAlignment.center,
            spacing: 12,
            runSpacing: 8,
            children: [
              ElevatedButton(
                onPressed: enabled ? controller.calibrate : null,
                style: ElevatedButton.styleFrom(
                  backgroundColor: mini_colors.darkRoyalPurple,
                  foregroundColor: mini_colors.offWhite,
                ),
                child: const Text('Calibrate'),
              ),
              ElevatedButton(
                onPressed: enabled ? controller.home : null,
                style: ElevatedButton.styleFrom(
                  backgroundColor: mini_colors.darkRoyalPurple,
                  foregroundColor: mini_colors.offWhite,
                ),
                child: const Text('Home'),
              ),
            ],
          ),
          const SizedBox(height: 8),
          Text(
            state.isCalibrated
                ? 'Last calibration: ${state.rangePulses} pulses (full travel)'
                : 'Not calibrated yet',
            style: const TextStyle(color: mini_colors.lightPurpleGrey),
          ),
          const SizedBox(height: 8),
        ],
      ),
    );
  }
}
