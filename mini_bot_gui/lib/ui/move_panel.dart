import 'package:flutter/material.dart';
import 'package:ocadi_bot_gui/app_constants/colors.dart' as mini_colors;
import 'package:ocadi_bot_gui/backend_services/drive_controller.dart';
import 'package:ocadi_bot_gui/speed_slider.dart';

import 'section_heading.dart';

/// Move-to-position panel: numeric target (cm), speed slider, Go (`MOVE`)
/// and Stop (`STOP`).
///
/// Validates that the target is >= 0. The device rejects out-of-range
/// targets with `EVT:ERR,OUT_OF_RANGE`, which is surfaced via [errorText].
class MovePanel extends StatefulWidget {
  const MovePanel({
    super.key,
    required this.controller,
    required this.enabled,
    this.errorText,
  });

  final DriveController controller;

  /// Whether MOVE/STOP commands may be sent.
  final bool enabled;

  /// Error text to display (e.g. from `EVT:ERR,OUT_OF_RANGE` or
  /// `EVT:ERR,NOT_HOMED`), or null if none.
  final String? errorText;

  @override
  State<MovePanel> createState() => _MovePanelState();
}

class _MovePanelState extends State<MovePanel> {
  final TextEditingController _targetController = TextEditingController();
  int _moveSpeed = 50;
  String? _validationError;

  @override
  void dispose() {
    _targetController.dispose();
    super.dispose();
  }

  void _onGoPressed() {
    final text = _targetController.text.trim();
    final value = double.tryParse(text);

    if (value == null || value < 0) {
      setState(() {
        _validationError = 'Enter a target position >= 0 cm';
      });
      return;
    }

    setState(() => _validationError = null);
    widget.controller.moveTo(value, _moveSpeed);
  }

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16.0),
      child: Column(
        children: [
          const SectionHeading(title: 'Move to position'),
          SizedBox(
            width: 200,
            child: TextField(
              controller: _targetController,
              enabled: widget.enabled,
              keyboardType: const TextInputType.numberWithOptions(
                  decimal: true),
              decoration: const InputDecoration(
                labelText: 'Target position (cm)',
                border: OutlineInputBorder(),
                isDense: true,
              ),
            ),
          ),
          const SizedBox(height: 8),
          Text(
            'Move speed: $_moveSpeed%',
            style: const TextStyle(color: mini_colors.lightPurpleGrey),
          ),
          SpeedSlider(
            min: 0,
            max: 100,
            divisions: 100,
            initialValue: _moveSpeed.toDouble(),
            onChangedCallback: (value) {
              setState(() {
                _moveSpeed = value.round();
              });
            },
          ),
          const SizedBox(height: 8),
          Wrap(
            alignment: WrapAlignment.center,
            spacing: 12,
            runSpacing: 8,
            children: [
              ElevatedButton(
                onPressed: widget.enabled ? _onGoPressed : null,
                style: ElevatedButton.styleFrom(
                  backgroundColor: mini_colors.darkRoyalPurple,
                  foregroundColor: mini_colors.offWhite,
                ),
                child: const Text('Go'),
              ),
              ElevatedButton(
                onPressed: widget.enabled ? widget.controller.stop : null,
                style: ElevatedButton.styleFrom(
                  backgroundColor: Colors.redAccent,
                  foregroundColor: Colors.white,
                ),
                child: const Text('Stop'),
              ),
            ],
          ),
          if (_validationError != null)
            Padding(
              padding: const EdgeInsets.only(top: 8.0),
              child: Text(
                _validationError!,
                style: const TextStyle(color: Colors.red),
              ),
            ),
          if (widget.errorText != null)
            Padding(
              padding: const EdgeInsets.only(top: 8.0),
              child: Text(
                'Device error: ${widget.errorText}',
                style: const TextStyle(
                    color: Colors.red, fontWeight: FontWeight.bold),
              ),
            ),
          const SizedBox(height: 8),
        ],
      ),
    );
  }
}
