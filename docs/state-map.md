# State Map

Current state map recovered from `AlexaConstants` in newer Intel NUC Software Studio packages.

Payload layout after raw MID `0x0603`:
- byte0: function = `1`
- byte1: state id
- byte2: speed
- byte3: brightness
- byte4: direction

In practice, current Linux path is confirmed with the first 4 bytes. `direction` is kept in the tool for completeness.

## Confirmed visual behavior

1. `off` / `blank`
- smooth fade out

2. `waking`
- lights from both edges toward the center
- steady state: purple edges with a cyan gradient toward the center

3. `active_direction`
- no visible autonomous effect during testing
- likely depends on voice/audio direction context

4. `active_ending`
- lights fully, then fades from edges toward the center
- looks like a closing animation for the active/talking state

5. `thinking`
- purple base
- cyan expands from center toward edges in a loop

6. `talking`
- purple and cyan smoothly replace each other in a loop

7. `mics_off_start`
- smooth red fade-in

8. `mics_off`
- sustained red state
- device returned `e3`, but visual state still changed

9. `mics_off_end`
- no visible autonomous effect during testing

10. `alert`
- cyclical cyan/blue alert animation converging toward center and expanding back out

11. `alert_short`
- same family as `alert`, but shorter burst pattern
- observed as about 4 seconds active, 2 seconds pause, then repeat

12. `notification_incoming`
- single soft yellow flash for around 2 seconds

13. `notification_unheard`
- slow pulsing yellow blink

14. `do_not_disturb`
- single lilac pulse

15. `bluetooth_connect`
- three quick blue flashes

16. `bluetooth_disconnect`
- visually matched the same three quick blue flashes in testing

17. `error`
- three smooth red pulses

## Notes
- Color palette appears broader than the early purple/cyan modes suggest:
  - red appears in microphone and error states
  - yellow appears in notification states
  - blue appears in Bluetooth states
- Some states likely depend on additional system context on Windows and may show richer behavior there.
