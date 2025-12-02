# **APPENDIX D – IEEE Documentation**

---

## **Page 1 (shows page number 89)**

**APPENDIX D   IEEE Documentation**

**REQUIRED DEVICE DOCUMENTATION
FOR IEEE 488.2-1987**

**General Information**
The 9210’s GPIB interface is IEEE 488.2-1987 compatible.
Section 4.9 of IEEE 488.2-1987 contains a list of device
documentation requirements. This section contains required
device documentation not covered elsewhere in the manual.
For items which are covered elsewhere, the appropriate
section of this manual is referenced.

**GPIB Interface
Function
Subsets**

1. The 9210 implements the following 488.2 Interface
    Function subsets:

SH1 – Source handshake complete capability
AH1 – Acceptor handshake complete capability
T6 – Basic talker, Serial poll, unaddress if MLA, no
   Talk Only
L3 – Basic listener, Unaddress is MTA, Listen Only
   mode
SR1 – Service request complete capability
RL1 – Remote/Local complete capability
PP0 – No parallel poll capability
DC1 – Device Clear (and Selected Device Clear) complete
   capability
DT1 – Device Trigger complete capability
C0 – No controller capability
E2 – Tri-state lines (except SRQ, NRFD, NDAC)

*89*

---

## **Page 2 (shows page number 90)**

**Appendix D  IEEE Documentation**

**Addressing
Information**

2. It is not possible to set the device’s address outside the
    range of 0 to 30.  An attempt to do so causes an error
    message to be displayed, indicating that the requested
    value is out of range, and the address is not changed.

3. A user initiated address change is recognized
    immediately.

**Restoration of
Settings**

4. The 9210’s device settings at power on are restored to
    the values they had when the 9210 was powered off.
    (Note that “device settings” is a standard-defined term.
    Other items, such as the status data structures and enable
    registers, are cleared at power on.)

**Commands and
Queries**

5.

a) The input buffer is 257 bytes, the last byte of which
  is inside a commercial integrated circuit which
  implements IEEE 488.1.  The input buffer cannot
  overflow. If the input buffer becomes filled, one
  byte is accepted as each byte is removed from the
  input buffer by the 9210’s parser.

b) The only query returning more than one
  <RESPONSE MESSAGE UNIT> is “ERR?”.
  Query responses are further documented in the
  Chapter 6 (Remote Operations) and Chapter 7 (GPIB
  Commands) of this manual.

c) All queries generate a response when parsed.

d) No queries generate a response when read.

*90*

---

## **Page 3 (shows page number 91)**

**IEEE Documentation  Appendix D**

e) The following commands are “coupled” to at least
  one other command:

AMP  Pulse Amplitude
BASE Pulse quiescent level
DEL  Pulse Delay
FREQ Frequency
LEAD Leading edge time
LOADComp Load Compensate
MED  Pulse median amplitude
    (BASE + AMP/2)
PHA  Phase
PER  Period
SLEW_Lead Slew rate, leading edge
SLEW_Trail Slew rate, trailing edge
TRAIL Trailing edge time
TRMD Trigger Mode
WID  Pulse Width
VHI  Pulse high level
VLO  Pulse low level

 The effect of “coupled” commands is described in
 Chapter 7 of this manual (GPIB Commands). All
 other commands are also documented in Chapter 7

**Device Specific
Commands**

6. The 9210’s device specific commands can be built using
    all functional elements defined in IEEE 488.2-1987
    section 7.3.3 except <EXPRESSION PROGRAM
    DATA>. It is never necessary to use <NON-
    DECIMAL NUMERIC PROGRAM DATA>.
    (Commands that accept <DECIMAL NUMERIC
    PROGRAM DATA> will also accept <NON-
    DECIMAL NUMERIC PROGRAM DATA> –
    time in picoseconds, volts in microvolts, dimensionless
    values in units). <ARBITRARY BLOCK
    PROGRAM DATA> elements, although accepted by
    the parser, are not used in any of the 9210’s commands
    and will therefore generate a command error.

*91*

---

## **Page 4 (shows page number 92)**

**Appendix D  IEEE Documentation**

**Data Elements**

7. The size of a block data element is limited by the size of
    the parser’s buffer.  The <PROGRAM MESSAGE
    UNIT> which contains the block data element must fit
    entirely within the parser’s 300 byte buffer.

 NOTE: At the moment no 9210 command uses
 <ARBITRARY BLOCK PROGRAM
 DATA> elements.

8. <EXPRESSION PROGRAM DATA> elements are
     not supported.

**Query
Responses**

9. The response syntax for every query is specified in
    Chapter 7 of this manual (GPIB Commands).

10. The 9210 does not send any message which does not
     comply with the rules for <RESPONSE
     MESSAGE> elements.

11. The 9210 does not produce any block data responses.

*92*

---

## **Page 5 (shows page number 93)**

**IEEE Documentation  Appendix D**

**Implemented
Commands and
Queries**

12. The following IEEE 488.2 common commands and
     queries are implemented:

 *CAL? *CLS *ESE *ESE? *ESR? *IDN?
 *LNR? *OPC *OPC? *OPT? *RCL *RST
 *SAV *SRE *SRE? *STB? *TRG *TST?
 *WAI

 This list includes fourteen mandatory commands and five
 optional commands. Further information on these
 commands can be found in Chapter 7 of this manual
 (GPIB Commands).

**State After
Calibration**

13. After calibration, the 9210 is automatically returned to its
     state before calibration.

**Identification
Response**

16. The response to *IDN is an <ARBITRARY ASCII
     RESPONSE DATA> element (an unquoted string) as
     specified by IEEE 488.2. It is of the form:

 LECROY,9210,0,1.2:910322

 The first field is the manufacturer. The second field is
 the model number. The third field is the serial number
 or 0 if not available. The fourth field is Firmware level
 or equivalent.

*93*

---

## **Page 6 (shows page number 94)**

**Appendix D  IEEE Documentation**

**Reset, Save,
Recall and
Learn**

19. The states affected by *RST, *SAV, *RCL and
     *LNR? are:

 Mainframe: Period, freq, trigger mode, trigger level,
 trigger slope, trigger input impedance, burst count,
 trigger out level

 For each module: width, duty cycle, delay, vhigh,
 vlow, amplitude, base, median, lead, trail, slew_ld,
 slew_tr, double pulse, invert, disable, load
 compensation, ECL termination, ECL termination
 voltage, limit, voltage max limit, voltage min limit.

 In addition to the above, the display format is affected.
 The display format selects the following: Vhigh and
 Vlow or amplitude and base or amplitude and median,
 period or frequency, width or duty cycle, lead and trail
 or slew lead and slew trail.

 NOTE: Some module dependent parameters may not
 be controllable on all modules. If a module is
 installed where some parameters cannot be
 controlled, those parameters which cannot be
 controlled are not saved by *SAV, not
 reported by *LNR?, and are not affected by
 *RCL or *RST.

*94*

---

## **Page 7 (shows page number 95)**

**IEEE Documentation  Appendix D**

**Selftest**

20. The scope of selftest performed by *TST is as follows:

**CALIBRATION**
Top level procedure:

**CAL_ADC:**
Purpose: Check ADC functionality. Note: not really a
 calibration.
Procedure: take 10 readings at ground, and nominal
 2.5V from resistive voltage divider.
Error bit: Bit 0 (value 1) set in *CAL? answer if error
Details (Error codes in cal msg and TER? 0):
1: More than 10 code spread from max to min reading
 at 1 voltage.
2: Out of limit: Ground > 1 code, or 2.5V > 2150 or <
 1945 (+/- 5% of 2048)
Displayed message: Either “Cal ADC… Passed” or
 “Cal ADC… Failed,” followed by the numeric code
 shown in “details” above, followed by a line showing
 the sum of the ten readings of 2.5V (should be
 approximately 20480) and the sum of the ten
 readings for Ground (should be approximately 0).

**TEST_FCNT:**
Purpose: Check frequency counter functionality
Procedure: Count 16 MHz clock (through TDC start
 MUX) for 3ms gate, 10 times.
Error bit: Bit 1 (value 2) set in *CAL? answer if error
Details (Error codes shown in cal msg and TER? 1):
1: Failed. Gate end interrupt did not occur within a
 reasonable number of milliseconds.
2: Excessive spread: max reading - min reading greater
 than 10 counts.
4: Out of limit: Average reading was not between
 47992 and 48008. These limits were chosen to
 account for 400ns gate error plus one count.

*95*

---

## **Page 8 (shows page number 96)**

**Appendix D  IEEE Documentation**

**CAL_TDC:**
Purpose: find code for 0 time (pedestal) and fs per code
 for fast tdc.
Note: fast tdc is time to voltage converter, then the 12
 bit ADC.
Procedure: Using the 16 MHz reference clock as start
 and stop, sum 1000 readings of 1 cycle time (use
 second stop = on), then sum 250 readings of
 pedestal reading, i.e, start and stop on same edge -
 multiply by four to scale as if 1000 readings. Save
 pedestal code * 1000, and tdc_fs_per_code = (sum
 of 1 cycle - (pedestal * 1000) ) / 625000000.
Error bit: Bit 2 (value 4) set in *CAL? answer if error.
Details (Error codes in cal msg and TER? 2):
1: Failed. TDC STOPPED interrupt did not occur
 within a reasonable number of milliseconds.
2: Excessive spread: >64 codes spread on pedestal
  readings, or >220 codes spread on 62.5ns readings.
4: Out of limit: Pedestal limits are 50 to 400, 62.5ns
  limits are 3000 to 4000 codes. Limit is checked on
  average of readings.
Displayed message: Either “Cal TDC… Passed” or “Cal
 TDC… Failed” followed by minimum, maximum
 and average readings for pedestal and 62.5ns, in
 codes.

**CAL_VCO:**
Purpose: This routine calibrates the 9210’s timing
 circuits. It is called five times, for PERIOD,
 DELAY_A, WIDTH_A, DELAY_B, and WIDTH_B
 timing circuits, in the order shown.
Procedure: The frequency counter is used to measure
 each VCO’s free-running frequency (prescaled by
 64) at 74 selected control voltage points. The points
 have been selected so that linear interpolation
 between the points will result in a maximum error of
 less than 0.05% for the expected control voltage vs
 frequency curve. The VCO frequency is varied from

*96*

---

## **Page 9 (shows page number 97)**

**IEEE Documentation  Appendix D**

maximum to minimum. The first count is made with
a gate time of 9ms, which should result in a count of
over 50000 (count of 65535 at 466 MHz, 50000 at
355.6 MHz) As the VCO is slowed down, the gate
time is increased by a factor of 1.25 whenever a
count under 30000 is seen. Since the expected
accuracy of the frequency counter is better than
400ns gate accuracy +/- 1 count, the calibrated points
should be accurate to better 1 part in 20000.
Error bit: Bit 4 in *CAL? response
Details: (TER? 4 response)
1: Unreasonable freq reading (less than 25000 counts)
 OR fastest reading > 3.5 ns or slowest reading
 ≤ 19 ns.
2: Non-monotonic vco frequency change
4: Aborted (very unreasonable reading, less than 20000
 counts)
Displayed message: Either “… Passed” or “… Failed”
 followed by either the fastest and slowest readings in
 ps if not aborted, or if aborted, the last frequency
 counter count.

**CAL_PLUGIN_A:**
Purpose: Calibrate amplitude, offset and slew rates.
Procedure: Different for each module type. Verifies all
 module specs except linearity.
Error bit: Bit 8 in *CAL? response
Details: (TER? 8 response)

**CAL_PLUGIN_B:**
Purpose: Calibrate amplitude, offset and slew rates.
Procedure: Different for each module type. Verifies all
 module specs except linearity.
Error bit: Bit 9 in *CAL? response
Details: (TER? 9 response)

**CAL_DISP_TEMPERATURE:**
Purpose: Append temperature to cal message.

*97*

---

## **Page 98** (first of your remaining uploads)

**Appendix D  IEEE Documentation**

CAL_DISP_REV_INFO:
Purpose: Append firmware revision information to cal
 message.

SELFTEST
 Top level procedure:

CALIBRATE:
 Performs calibration (see above).

STEST_MAIN_PROM:
 Purpose: sumcheck main Prom
 Procedure: Add bytes to 32 bit sum. Start: 0. End:
  0x1FFFB. If 1FFFC to 1FFFF is not 0, assume it
  is expected checksum. Compare, show PASS or
  FAIL. If it is zero, just display computed checksum
  – never fail.
 Error bit (in *TST? response): ERRBIT_STEST, bit
  15
 Details: (in TER? 15 response): 4

STEST_PLUGIN_PROM:
 Purpose: sumcheck plugin’s Prom. Called twice, for A
  and B.
 Procedure: Add bytes to 32 bit sum. If plugin installed:
  For plugin A Start: 0x80000. End: 0x87FFB.
  For plugin B Start: 0x88000. End: 0x8FFFB.
  If 4 bytes following END are not 0, assume they are
  expected checksum. Compare, show PASS or
  FAIL. If zero, just display computed checksum –
  never fail.
 Error bit: ERRBIT_STEST
 Details: (in TER? 15 response):
  8 = plugin A checksum failed
  16 = plugin B checksum failed

*98*

---

## **Page 99**

**IEEE Documentation  Appendix D**

STEST_BBRAM:
 Purpose: perform a ram test on battery backed up
  RAM, from 7C000 to 7FFFF, 8K bytes.
 Procedure: for as many 1K blocks as necessary:
  Disable processor interrupts
  Save 1K byte block
  Write block with start pattern, low to high addr
   (forward)
  Read forward and compare start pattern
  Write backward complement of start pattern
  Read backward and compare complement pattern
  Restore block’s original contents
  Enable interrupts
  check_delays() – updates watchdog
 Note: this test is based on “Efficient algorithms for
  Testing Semiconductor Memories” by R. Nair et al,
  IEEE Transactions on Computers Vol. C-27 No. 6,
  June 1978. This test will catch any stuck data line,
  cell fault or coupling and, as implemented, faults in
  the lowest 10 address lines or in the RAM chip’s
  decoding of these lines.
 Error bit: ERRBIT_STEST
 Details: (in TER? 15 response): 1

STEST_RAM:
 Purpose: same as stest_bbram, but for main ram at
  40000 to 47FFF.
 Error bit: ERRBIT_STEST
 Details: (in TER? 15 response): 2

STEST_VIDEO:
 Purpose: Verify that Horizontal and Vertical Sync
  signals are being generated and are approximately the
  expected frequency.
 Procedure: Run frequency counter: source Hsync,
  10ms gate. Check that result is 189 to 231, ie, 18.9
  to 23.1 kHz. Run frequency counter: source Vsync,
  800ms gate. Check that result is 35 to 50, ie, 43.75
  to 62.5 Hz.

*99*

---

## **Page 100**

**Appendix D  IEEE Documentation**

Error bit: ERRBIT_STEST
Details: (in TER? 15 response): 32

TRIGGER_CIRCUIT_TEST:
 Purpose: make sure trigger input comparators work,
  both slopes
 Procedure: Save trigger mode, trigger slope, trigger
  level. Set trigger mode to External Width. Turn on
  “trigger test” – ground trigger input through FET.
  For positive slope, negative slope, and trigger input
  disabled:
   Set the trigger comparator threshold to 2.5V. Start
   the tdc on output of Period generator. Swing the
   trigger input threshold to –2.5V. Expect trigger if
   trigger slope positive, else not. Start the tdc again on
   the output of the period generator. Swing the trigger
   input threshold back to +2.5V. Expect trigger if
   trigger slope negative, else not. Check that tdc fired
   when expected and timed out otherwise.
  Restore trigger mode, trigger slope, trigger level
 Error bit: ERRBIT_STEST
 Details: (in TER? 15 response): 64

TRIGGER_DELAY_TEST (errbit_delay,0):
 Purpose: Make sure that Period->Width->Delay chain is
  unbroken, with a reasonably short fixed delay.
 Procedure: Save trigger mode, trigger slope, and A and
  B width and delay. Set trigger mode SINGLE,
  slope DISABLED, and Channel A and B widths to
  100ns, and delays to 0.
  For channel A and B: Set tdc mux to start on Period
  out and stop on Channel (width up). Sum 64 tdc
  readings, using manual trigger to fire period gen.
  Check that tdc never always fired and reading
  < 20ns (approx). Restore trigger mode, trigger
  slope, and A and B width and delay
 Error bit: ERRBIT_DELAY = bit 3

*100*

---

## **Page 101**

**IEEE Documentation  Appendix D**

Details: CALERR_PROPA_DELAY (1) or
 CALERR_PROPA_DELAY_B (2)

**Status Data
Structure**

21. The status data structures in the 9210 are:
     Status Byte Register: Contains status summary
      messages. The 488.2 standard defines the following
      bits:

 MAV – Message Available – true when the output
    queue is not empty.
 ESB – Event Status Bit – true when an enabled bit in
    the Standard Event Status Register has been
    set since the last reading or clearing of the
    Standard Event Status Register (see below).
 RQS – Indicates that this device is requesting
    service. This bit is only readable by serial
    poll, and is cleared after being read once.
 MSS – Indicates that an enabled bit in the status byte
    register is true. This bit replaces RQS when
    the status byte is read by “*STB?”, as
    shown in the diagram below.

 In addition, we define:

 ERQ – Error Queue summary bit – true when the
    error queue is not empty.

 Service Request Enable Register: Each bit (except
  bit 6) of this register “enables” the corresponding bit in
  the Status Byte Register when true. Bit 6 in the Status
  Byte Register cannot be disabled. This register is
  completely defined by 488.2.

*101*

---

## **Page 102**

**Appendix D  IEEE Documentation**

Standard Event Status Register: IEEE 488.2
defines all of the bits in this register. It is read by
*ESE?.

 Bit 7 – Power on
 Bit 6 – User Request (not used by 9210)
 Bit 5 – Command Error
 Bit 4 – Execution Error
 Bit 3 – Device Dependent Error
 Bit 2 – Query Error
 Bit 1 – Request Control (not used by 9210)
 Bit 0 – Operation Complete (used for *OPC
   command)

Standard Event Status Enable Register: Defined
by 488.2. Each bit “enables” the corresponding bit of
Standard Event Status Register when 1. If any enabled
bit becomes set, the Event Summary Bit (ESB) in the
Status Byte register becomes set.

Output Queue: defined in 488.2. Our implementation
holds 257 bytes. If a larger response needs to be
generated, the 9210 finishes placing the response in the
buffer as the first part is read out.

In addition to these registers and the output queue,
which are defined by 488.2, we define an Error Queue.
The Error Queue summary bit is bit 7 in the Status Byte
Register, and has already been discussed. The error
queue holds 31 entries. It is read by the ERR? query.

*102*

---

## **Page 103**

**IEEE Documentation  Appendix D**

Successive readings get successive entries. If the queue
is empty, ERR? returns 0,“NO ERROR”. If the
error queue is full when an error occurs, then ERR?
will return 350,“TOO MANY EVENTS” after
returning the 31 entries in the queue, (ie on the 32nd
query). A complete list of error codes appears in the
Appendix E of this manual.

**STATUS BYTE AND SERVICE REQUEST ENABLE REGISTER LAYOUT**

```
              7   6   5   4   3   2   1   0
Status Byte Register
(as read by Serial
 Poll)          ERQ RQS ESB MAV 0   0   0   0
(as read by *STB?) ERQ MSS ESB MAV 0 0 0 0

Service Request
Enable Register   7   X   5   4   3   2   1   0
(read by *SRE?,
written by *SRE <NRs>)
```

**Sequential
Processing**

22. All 9210 commands are sequential, the 9210 has no
     “overlapped” commands. The only exception to strict
     sequential processing is for coupled commands (see 5e,
     above) as described in 488.2-1987 section 6.4.5.3.

**Operation
Complete**

23. Op Complete is generated when all <PROGRAM
     MESSAGE UNITS> in a <PROGRAM
     MESSAGE> have completed execution. Op Complete
     generation is the final action, after the completion of
     coupled commands (if any), caused by a <PROGRAM
     MESSAGE>. The GPIB Commands Chapter of this
     manual (Chapter 7) documents the functional criteria that
     are met by each command.

*103*

---

## **Page 104**

**Appendix D  IEEE Documentation**

**Additional
Notes**

I) Items 14, 15, 17, 18 are only required if certain optional
 common commands (macro commands, *PUD, and
 *RDT) are implemented. The 9210 does not implement
 these commands.

II) The above required documentation is correct for the set of
 documented 9210 commands. The 9210 contains a few
 headers which are meant only for testing and are not
 documented in this manual.

*104*

