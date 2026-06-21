import 'dart:async';

import 'package:flutter/material.dart';
import 'package:ocadi_bot_gui/app_constants/colors.dart' as mini_colors;
import 'package:ocadi_bot_gui/backend_services/drive_controller.dart';
import 'package:ocadi_bot_gui/models/drive_state.dart';
import 'package:ocadi_bot_gui/speed_slider.dart';

import 'section_heading.dart';

/// Jog panel: hold-to-jog buttons toward MIN/MAX plus a 0-100 speed
/// slider.
///
/// While a button is held, repeatedly sends `JOG:F|B,<speed>` every
/// ~100 ms (the protocol requires a keepalive re-send within 250 ms);
/// on release sends `JSTOP`.
class JogPanel extends StatefulWidget {
  const JogPanel({
    super.key,
    required this.controller,
    required this.state,
    required this.enabled,
  });

  final DriveController controller;
  final DriveState state;

  /// Whether jogging is allowed at all (e.g. false while e-stop latched
  /// or while a re-home is required and both directions would be unsafe).
  final bool enabled;

  @override
  State<JogPanel> createState() => _JogPanelState();
}

class _JogPanelState extends State<JogPanel> {
  static const Duration _keepaliveInterval = Duration(milliseconds: 100);

  int _jogSpeed = 50;
  Timer? _jogTimer;
  JogDirection? _activeDirection;

  @override
  void dispose() {
    _jogTimer?.cancel();
    super.dispose();
  }

  void _startJog(JogDirection direction) {
    if (!widget.enabled) return;

    _activeDirection = direction;
    widget.controller.jog(direction, _jogSpeed);

    _jogTimer?.cancel();
    _jogTimer = Timer.periodic(_keepaliveInterval, (_) {
      if (_activeDirection == direction) {
        widget.controller.jog(direction, _jogSpeed);
      }
    });
  }

  void _stopJog() {
    _jogTimer?.cancel();
    _jogTimer = null;
    if (_activeDirection != null) {
      _activeDirection = null;
      widget.controller.jogStop();
    }
  }

  Widget _jogButton({
    required String label,
    required JogDirection direction,
    required bool disabled,
  }) {
    return GestureDetector(
      onTapDown: disabled ? null : (_) => _startJog(direction),
      onTapUp: disabled ? null : (_) => _stopJog(),
      onTapCancel: disabled ? null : _stopJog,
      child: Container(
        width: 160,
        height: 64,
        alignment: Alignment.center,
        decoration: BoxDecoration(
          color: disabled
              ? mini_colors.notQuiteWhite
              : mini_colors.darkRoyalPurple,
          borderRadius: BorderRadius.circular(8),
        ),
        child: Text(
          label,
          textAlign: TextAlign.center,
          style: TextStyle(
            color: disabled
                ? mini_colors.lightPurpleGrey
                : mini_colors.offWhite,
            fontWeight: FontWeight.bold,
            fontSize: 16,
          ),
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final disabled = !widget.enabled;

    // If an unexpected limit was hit, disable jogging further into that
    // same endstop until the user re-homes (the opposite direction
    // remains available, per protocol).
    final backDisabled =
        disabled || (widget.state.needsRehome && widget.state.limitSide == LimitSide.min);
    final forwardDisabled =
        disabled || (widget.state.needsRehome && widget.state.limitSide == LimitSide.max);

    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16.0),
      child: Column(
        children: [
          const SectionHeading(title: 'Jog'),
          Wrap(
            alignment: WrapAlignment.center,
            spacing: 16,
            runSpacing: 12,
            children: [
              _jogButton(
                label: 'Jog Back\n(toward MIN)',
                direction: JogDirection.backward,
                disabled: backDisabled,
              ),
              _jogButton(
                label: 'Jog Forward\n(toward MAX)',
                direction: JogDirection.forward,
                disabled: forwardDisabled,
              ),
            ],
          ),
          const SizedBox(height: 8),
          Text(
            'Jog speed: $_jogSpeed%',
            style: const TextStyle(color: mini_colors.lightPurpleGrey),
          ),
          SpeedSlider(
            min: 0,
            max: 100,
            divisions: 100,
            initialValue: _jogSpeed.toDouble(),
            onChangedCallback: (value) {
              setState(() {
                _jogSpeed = value.round();
              });
            },
          ),
        ],
      ),
    );
  }
}
