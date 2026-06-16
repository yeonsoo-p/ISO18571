# In-House Qualification of Virtual Euro NCAP Frontal Collision Avoidance Tests for Vans

Status: informational draft based on local documentation in this directory.

Scope: this note describes an internal workflow for qualifying simulation-based Euro NCAP frontal collision avoidance predictions for Commercial Vans (CV / N1), with emphasis on van frontal collision avoidance scenarios. The current local Euro NCAP documentation does not provide a van-specific virtual testing qualification protocol. Where the van protocol is silent, this note redirects to the passenger-car frontal collision and virtual testing documentation and marks the dependency explicitly.

This document is not a substitute for Euro NCAP approval. It is intended to help prepare an in-house qualification dossier, organize physical and virtual data consistently, and make gaps visible before manual research or Euro NCAP clarification.

## 1. Source Documents Reviewed

| Local document | Role in this note | Applicability |
| --- | --- | --- |
| `EuroNCAP 2026/euro_ncap_cv_protocol_crash_avoidance_frontal_collisions_v10_cd7edfb7af.pdf` | Primary van frontal collision avoidance protocol: definitions, measuring equipment, test conditions, VUT loading, scenario matrices, performance predictions, verification tests, assessment KPIs, and current CV scoring. | Confirmed for vans. |
| `EuroNCAP 2026/euro_ncap_cv_protocol_overall_assessment_v10_4e55a1c2d7.pdf` | CV overall rating structure and Crash Avoidance point allocation. | Confirmed for vans. |
| `EuroNCAP 2026/euro_ncap_cv_protocol_test_vehicle_and_variants_policy_v20_a72e4b29fb.pdf` | CV vehicle selection, variants, and additional evidence expectations. | Confirmed for vans. |
| `EuroNCAP 2026/euro_ncap_supporting_protocol_safe_driving_crash_avoidance_virtual_testing_v10_bb5738fef8.pdf` | Euro NCAP virtual testing process, simulation model requirements, ISO score and KPI qualification method. | Passenger-car/Safe Driving and Crash Avoidance VTA reference; not van-specific in the local docs. |
| `EuroNCAP 2026/euro_ncap_protocol_crash_avoidance_frontal_collisions_v11_bc661b4bdc.pdf` | Passenger-car standard, extended, robustness layer prediction and scoring rules. | Use by analogy where CV protocol has no standard/extended/robustness split. |
| `EuroNCAP 2026/CA_004_Data_Acquisition_and_Assessment_Criteria_Calculation_v1_4_c2590c51f5.pdf` | ISO-MME folder structure, MME headers, channel naming, filtering, and assessment-criteria calculations. | Applies to Safe Driving and Crash Avoidance data generally; current scenario table appears car-code only. |
| `EuroNCAP 2026/ca_002_verification_conditions_for_robustness_layers_v11_4be423649d.pdf` | Robustness layer verification conditions for frontal collisions and lane departure. | Passenger-car frontal robustness reference; not van-specific in the local docs. |
| `EuroNCAP 2026/ca_003_field_data_template_v10_912c89da49.pdf` | Field-data evidence template referenced for perception robustness in passenger-car protocol. | Reference when field-data evidence is requested. |
| `ISO_TS_18571_2024(en).pdf` | Objective rating metric for comparing physical and simulation time-history signals. | Applies to ISO score calculation referenced by Euro NCAP VTA. |
| `ISO_TS_13499_2019(en).pdf` and `ISO TS13499 RED V16/*` | ISO-MME data format and channel coding references. | Applies to physical and virtual data organization. |
| `ISO_8855_2011(en).pdf` | Vehicle coordinate system convention. | Applies to reference-system definition. |
| `ISO_19206-1/2/3/4/5_*.pdf` | Target specifications for vehicle, pedestrian, bicyclist, and motorcyclist targets. | Applies to physical and virtual target equivalence. |

## 2. Key Finding

The local CV frontal protocol currently defines the physical van assessment and scoring, but it does not define a van-specific VTA qualification scheme, van-specific ISO/KPI acceptance thresholds, or van standard/extended/robustness scoring. The passenger-car frontal protocol and the Euro NCAP VTA supporting protocol do define those concepts for car scenarios. Until Euro NCAP publishes a CV VTA protocol, in-house work should:

1. Use the CV frontal protocol for van definitions, vehicle preparation, scenario matrices, boundary conditions, verification tests, assessment KPIs, and current official CV score calculation.
2. Use the Euro NCAP VTA supporting protocol for simulation model qualification principles, ISO score calculation, KPI error calculation, and VTA acceptance workflow.
3. Use the passenger-car frontal protocol only as a reference for standard range, extended range, robustness layer prediction, verification, and scoring logic.
4. Clearly label every passenger-car-derived assumption in the dossier as provisional for CV and requiring Euro NCAP confirmation.

## 3. Definitions and Terms

Use the definitions in the CV frontal protocol where a van term exists.

| Term | Working definition for this guide |
| --- | --- |
| VUT | Vehicle under test. For this guide, the van being assessed. |
| GVT | Global Vehicle Target, specified by ISO 19206-3 and used for van-to-car cases. |
| EPTa / EPTc | Adult and child pedestrian targets specified by ISO 19206-2. |
| EBTa | Adult bicyclist target specified by ISO 19206-4. |
| EMT | Motorcyclist target specified by ISO 19206-5. |
| AEB | Automatic braking intervention in response to a likely collision. |
| FCW | Audio-visual warning in response to a likely collision. |
| TAEB | Time at which AEB activation is identified from the filtered longitudinal acceleration trace. |
| TFCW | Time at which the audible FCW starts. |
| T0 | Start of test. In the CV frontal protocol, generally TTC = 4 s unless scenario-specific timing applies. |
| Tend | End of test as defined by the applicable physical-test protocol. |
| Vimpact | VUT speed at impact with the target virtual box/profile definition. |
| Vrel_impact | Relative speed at impact, calculated from VUT speed minus target speed. |
| Vreduction | VUT speed reduction from T0 to impact. |
| Grid cell | A single combination of scenario parameters such as VUT speed, target speed, impact location, headway, or target acceleration. |
| Standard range | Passenger-car term: most basic controlled grid cells for a scenario. Not yet defined for CV frontal in the local documents. |
| Extended range | Passenger-car term: minor added complexity such as impact-position or speed variations. Not yet defined for CV frontal in the local documents. |
| Robustness layer | Passenger-car term: added complexity intended to challenge real-world reliability. Not yet defined for CV frontal in the local documents. |
| ISO score | In Euro NCAP VTA, the ISO/TS 18571 objective rating for the longitudinal VUT acceleration channel, comparing physical and simulated signals. |
| KPI error | Difference between physical-test KPI and simulation KPI for the same test case. |

## 4. Applicability to Vans

### 4.1 Confirmed CV frontal scenarios

The CV frontal protocol allocates 65 Crash Avoidance points to Frontal Collisions in the CV overall assessment: 40 points for Car and PTW scenarios, and 25 points for Pedestrian and Cyclist scenarios.

For van-to-car and van-to-PTW, the CV frontal protocol defines:

| Scenario group | CV scenarios | Current CV points |
| --- | --- | --- |
| Van-to-Car Rear | `VCRs`, `VCRm`, `VCRb` | 10.0 |
| Van-to-Car Front | `VCFhos`, `VCFhol` | 5.0 |
| Van-to-Motorcycle Rear | `VMRs`, `VMRb` | 5.0 |
| Van-to-Car Turn Across Path | `VCFtap` | 5.0 |
| Van-to-Motorcycle Turn Across Path | `VMFtap` | 5.0 |
| Van-to-Car Crossing | `VCCscp` | 10.0 |

The CV protocol also defines pedestrian and bicyclist frontal scenarios such as `VPFA`, `VPNA`, `VPNCO`, `VPLA`, `VPTA`, `VBNA`, `VBNAO`, `VBFA`, `VBLA`, and `VBTA`.

### 4.2 Passenger-car fallback mapping

When virtual-test qualification details are absent for vans, use the passenger-car documents only as a reference. A practical mapping is:

| CV scenario family | Passenger-car reference family | Use |
| --- | --- | --- |
| `VCRs`, `VCRm`, `VCRb` | `CCRs`, `CCRm`, `CCRb` | Longitudinal rear car-target behavior, KPI categories, VTA comparison method. |
| `VCFhos`, `VCFhol` | `CCFhos`, `CCFhol` | Head-on speed-reduction assessment and dossier expectations. |
| `VCFtap` | `CCFtap` | Turn-across-path test synchronization and avoidance scoring. |
| `VCCscp` | `CCCscp` | Straight crossing path setup, time-error logic, and crossing assessment. |
| `VMRs`, `VMRb`, `VMFtap` | `CMRs`, `CMRb`, `CMFtap` | PTW target logic and KPI families. |
| `VPLA`, `VPTA`, `VPNA`, etc. | `CPLA`, `CPTA`, `CPNA`, etc. | Pedestrian scenario families and AEB/FCW KPIs. |
| `VBLA`, `VBTA`, `VBNA`, etc. | `CBLA`, `CBTA`, `CBNA`, etc. | Bicyclist scenario families and AEB/FCW KPIs. |

This mapping is not an official Euro NCAP CV VTA rule in the local documentation. Treat it as a traceability aid for internal qualification planning.

## 5. Data Acquisition, Processing, and Organization

### 5.1 Coordinate system

Use ISO 8855 convention with the origin at the most forward point on the VUT centerline. Apply the same convention to physical and virtual data. For right-hand drive vehicles, ensure nearside/farside interpretation is swapped consistently relative to the left-hand-drive illustration in the protocols.

### 5.2 Physical data acquisition

For physical tests, follow the CV frontal protocol and CA 004:

1. Sample and record dynamic data at not less than 100 Hz.
2. Synchronize VUT and target data using DGPS time stamp.
3. Record VUT and target position, speed, acceleration, yaw rate, yaw angle, and any scenario-specific control signals required by CA 004.
4. Record from at least 0.5 s before T0 to 0.5 s after Tend where CA 004 applies. For pedestrian and bicyclist turning/crossing, CA 004 requires the recording window to begin before target acceleration.
5. Use SI units unless a protocol field explicitly requires another unit such as km/h.

Measurement accuracy from the CV frontal protocol should be retained in the internal checklist:

| Quantity | Required accuracy |
| --- | --- |
| VUT and target speed | 0.1 km/h |
| VUT and target lateral/longitudinal position | 0.03 m |
| VUT heading angle | 0.1 deg |
| VUT and target yaw rate | 0.1 deg/s |
| VUT and target longitudinal acceleration | 0.1 m/s2 |
| VUT steering wheel velocity | 1.0 deg/s |

### 5.3 Physical data filtering

Use the same filtering approach as the physical protocols and CA 004:

| Channel type | Processing |
| --- | --- |
| Position | Raw, unfiltered |
| Speed | Raw, unfiltered |
| Acceleration | 12-pole phaseless Butterworth, 10 Hz cutoff |
| Yaw rate | 12-pole phaseless Butterworth, 10 Hz cutoff |
| Steering wheel velocity | 12-pole phaseless Butterworth, 10 Hz cutoff |
| Force | 12-pole phaseless Butterworth, 10 Hz cutoff |

### 5.4 Virtual data output

For virtual testing, follow the VTA supporting protocol:

1. Export all required channels between T0 and Tend for the protocol-specific test case.
2. Output time-series data at 100 Hz.
3. Do not apply additional output filtering to simulation data.
4. Use ISO-MME channel names consistent with CA 004 and the physical test data.
5. Ensure values are physically plausible. Euro NCAP VTA states that data outside physical limits can be rejected.

Recommended internal practice: preserve a master simulation export with the same 0.5 s pre/post margins used for physical data, then create a VTA upload subset covering the required T0-to-Tend interval.

### 5.5 ISO-MME structure

Organize each physical or virtual run using ISO-MME 1.6 and ISO/TS 13499 conventions.

Minimum test-folder contents from CA 004:

```text
<test folder>/
  Channel/
    <test number>.xxx
    <test number>.chn
  Movie/
    <test number>_<movie name>
  <test number>.mme
  <test number>.txt
```

The `.mme` file should be UTF-8 encoded and include, at minimum, the relevant CA 004 headers:

| Header group | Examples to verify |
| --- | --- |
| Test identity | Data format edition, laboratory, customer, test-series number, title, timestamp, region. |
| Scenario identity | Scenario, type of test, subtype, run repetition, completion state. |
| Robustness layer | Type code, layer code, parameter code. Use `NOVALUE` if not applicable. |
| VUT details | Make/model, driver position, VIN, software version, dimensions, VUT shape, front overhang. |
| Scenario parameters | VUT velocity, target velocity, target acceleration, target heading, impact location. |
| Data source | `Physical Test` or `Virtual Test`. |

Important gap: CA 004 v1.4 includes car scenario codes such as `CCRs`, `CCRm`, `CCFtap`, `CCCscp`, `CPLA`, and `CBNA`, but the extracted local text did not show van scenario codes such as `VCRs`, `VCFtap`, or `VCCscp`. For a van dossier, confirm the approved CV scenario-code headers with Euro NCAP before relying on automated ingestion.

### 5.6 Required channels for frontal collision avoidance

Use CA 004 channel codes for both physical and virtual data. Core channels include:

| Object | Data | CA 004 channel pattern |
| --- | --- | --- |
| VUT | Position X/Y | `10VEHC000000DS[X,Y]P` |
| VUT | Speed X/Y | `10VEHC000000VE[X,Y]P` |
| VUT | Acceleration X/Y | `10VEHC000000AC[X,Y]P` |
| VUT | Yaw velocity | `10VEHC000000AVZP` |
| VUT | Yaw angle | `10VEHC000000ANZP` |
| VUT | FCW activation | `10TFCW000000EV00` |
| GVT | Position X/Y | `20VEHC000000DS[X,Y]P` |
| GVT | Speed X/Y | `20VEHC000000VE[X,Y]P` |
| GVT | Acceleration X | `20VEHC000000ACXP` |
| Pedestrian target | Position/speed/acceleration/yaw | `20PED[A,C]...` |
| Bicyclist target | Position/speed/acceleration/yaw | `20CYCL...` |
| Motorcycle target | Position/speed/acceleration/yaw | `20TWMB...` |

For virtual frontal or lane-departure predictions, CA 004 indicates that steering wheel, accelerator pedal, brake pedal, and turn-indicator channels are not requested for the VUT.

Note: CA 004 examples use physical-test channel suffixes such as `...P`, while the VTA supporting protocol identifies the simulation acceleration channel for ISO scoring as `10VEHC000000ACXS`. Keep physical and virtual channels traceably paired, and confirm the required suffix convention before external upload.

## 6. Simulation Model Requirements

The VTA supporting protocol defines model requirements that should be reused for van internal qualification unless Euro NCAP publishes CV-specific VTA requirements.

### 6.1 Model consistency

Keep the following model components unchanged across the qualification and prediction campaign unless the change is explicitly versioned, justified, and requalified:

1. Vehicle simulation model.
2. Target simulation models.
3. Environmental model.
4. Sensor model and sensor placement.
5. Perception assumptions.
6. Function algorithm model.
7. Vehicle dynamics model.

### 6.2 Vehicle model

The van simulation model should represent the assessed vehicle configuration:

1. Use van dimensions, mass, width, wheelbase, front overhang, VUT shape/profile, and as-tested loading condition from the CV protocol.
2. Match physical sensor locations, opening angles, azimuth, and range.
3. If using perfect perception, state that assumption explicitly and ensure it is consistent with the VTA supporting protocol.
4. Use a function algorithm comparable to the production vehicle function.
5. Include relevant vehicle-dynamics behavior for the frontal scenario family under assessment.

### 6.3 Target model

Virtual targets should match the physical targets in dimensions, reference points, virtual bounding boxes, and scenario-specific impact definitions:

1. GVT for van-to-car scenarios.
2. EMT or approved real motorcycle target for van-to-motorcyclist scenarios.
3. EPTa/EPTc for van-to-pedestrian scenarios.
4. EBTa for van-to-bicyclist scenarios.

### 6.4 Environment model

The environmental model should represent the nominal conditions of the physical protocol:

1. Dry, uniform, paved test surface.
2. Required friction level.
3. Weather and illumination conditions.
4. Lane and junction markings where relevant.
5. Free-space and surroundings requirements.
6. Obstructions or robustness-layer objects where relevant.

## 7. Physical-Test Validity Before Simulation Comparison

Before comparing simulation to physical data, validate that the physical run itself is usable.

### 7.1 Vehicle preparation

For CV frontal tests, verify:

1. AEB/FCW settings are configured per protocol, generally middle setting or next latest setting.
2. DSM influence on AEB/FCW sensitivity is deactivated if required.
3. Deployable pedestrian/VRU protection systems are deactivated if required.
4. Tyres are original fitment or identical replacements, inflated to the required pressure, and run-in per procedure.
5. Wheel alignment is recorded in kerb-weight condition.
6. Van loading follows the CV half-laden method:
   - Test-ready mass = unladen kerb mass + 200 kg interior load.
   - As-tested mass = test-ready mass + half of the remaining payload to GVW.
   - Final axle loads are recorded.

### 7.2 Track and environmental conditions

Confirm the CV protocol requirements:

1. Dry surface, no visible moisture.
2. Maximum longitudinal slope within +/-1 percent.
3. Maximum lateral slope within +/-3 percent.
4. Peak braking coefficient at least 0.9.
5. Ambient temperature above 5 deg C and below 40 deg C.
6. No precipitation.
7. Ground-level visibility greater than 1 km.
8. Wind speed below 10 m/s.
9. Daylight illumination above 2000 lux where daylight testing is required.
10. Required free-space area around VUT, target, and visual axis is clear.

### 7.3 Boundary conditions

For each run, check the protocol-specific boundary conditions from T0 until TAEB and/or TFCW. The CV protocol includes boundary checks for:

1. VUT speed tolerance.
2. Target speed tolerance.
3. VUT and target lateral deviation.
4. Lateral velocity for relevant target types.
5. Relative distance or time gap.
6. Yaw velocity before steering for turning tests.
7. Steering wheel velocity before steering for turning tests.
8. Scenario-specific path or synchronization errors, such as VUT longitudinal path error or SCP time error.

Any run outside boundary conditions should not be used for simulation qualification unless Euro NCAP or the internal sign-off authority accepts a documented exception.

## 8. Scenario Validation: Simulated vs Physical

Use a two-level validation process:

1. Scenario validity: physical and simulation runs must represent the same scenario grid cell, same parameter set, same VUT configuration, same target geometry, same reference system, and same test interval.
2. Result validity: simulation outputs must match physical results using Euro NCAP VTA-style ISO score and KPI error checks.

### 8.1 Scenario identity check

For each physical/simulation pair, verify:

| Item | Check |
| --- | --- |
| Scenario code | Same CV scenario or documented passenger-car analog. |
| Function | AEB, FCW, AES, ESS, or other declared function is identical. |
| VUT speed | Same nominal value and within physical-test tolerance. |
| Target speed | Same nominal value and within physical-test tolerance. |
| Impact location | Same nominal target/VUT impact definition. |
| Headway / acceleration | Same for braking scenarios such as `VCRb` and `VMRb`. |
| Direction / approach side | Same nearside/farside, turn direction, or crossing direction. |
| Obstruction / robustness layer | Same if present; otherwise both absent. |
| Vehicle configuration | Same software, sensors, system settings, load state, tyres, and relevant dimensions. |
| Data source | Physical and virtual are both represented in ISO-MME using consistent channels. |

### 8.2 Result KPIs for CV frontal assessment

The CV frontal protocol evaluates AEB/FCW performance using these assessment KPIs:

| Criteria family | KPI | CV examples |
| --- | --- | --- |
| Mitigation or avoidance | `Vrel_impact` | `VCRm`, `VPLA-50`, `VBLA-50` |
| Mitigation or avoidance | `Vimpact` | `VCRs`, `VCRb`, `VMRs`, `VMRb`, `VCCscp`, crossing VRU scenarios |
| Mitigation | `Vreduction` | `VCFhos`, `VCFhol` |
| Avoidance | `Vimpact` | `VCFtap`, `VMFtap`, `VPTA`, `VBTA` |
| Warning | FCW TTC | `VPLA-25`, `VBLA-25` |

For virtual qualification, compare the physical and simulated value for the applicable KPI(s) in the same grid cell. Use:

```text
KPI_error = KPI_physical - KPI_simulation
```

The VTA supporting protocol provides provisional KPI-error acceptance examples for passenger-car clusters. Bracketed values in that protocol are explicitly noted as requiring confirmation, so do not treat them as final CV thresholds:

| VTA cluster | Passenger-car KPI-error examples |
| --- | --- |
| Frontal longitudinal | TTC_AEB, TTC_FCW, remaining distance, impact speed |
| Frontal turning | TTC_AEB, TTC_FCW, impact speed |
| Frontal crossing | TTC_AEB, TTC_FCW, impact speed |
| Lane ELK | DTLE_ELK |

For vans, map the CV scenario to the closest passenger-car cluster and document the mapping. Example: `VCRs` maps to frontal-longitudinal `CCRs`; `VCFtap` maps to frontal-turning `CCFtap`; `VCCscp` maps to frontal-crossing `CCCscp`.

## 9. ISO/TS 18571 Score Validation

### 9.1 Signal and interval

The Euro NCAP VTA supporting protocol specifies the ISO score for the VUT longitudinal acceleration channel:

```text
10VEHC000000ACXS
```

Before the ISO/TS 18571 comparison:

1. Time-shift the simulation trace so that TAEB aligns with the physical test TAEB.
2. Evaluate from TAEB - 0.2 s to Tend.
3. If the physical and virtual Tend differ, use the earlier Tend.
4. Use ISO/TS 18571:2024 to calculate the objective rating.

### 9.2 ISO metric interpretation

ISO/TS 18571 produces an objective rating from 0 to 1, where higher values indicate better signal agreement. It combines corridor, phase, magnitude, and slope sub-ratings into one overall rating.

ISO/TS 18571 grade bands:

| Grade | Rating range |
| --- | --- |
| Excellent | R > 0.94 |
| Good | 0.80 < R <= 0.94 |
| Fair | 0.58 < R <= 0.80 |
| Poor | R <= 0.58 |

The Euro NCAP VTA supporting protocol shows bracketed acceptance examples of `0.5` for standard and extended range frontal clusters. Because the document itself says bracketed values require confirmation, record those as provisional only.

### 9.3 Qualification decision

For each physical/simulation pair:

1. Confirm the pair passed scenario identity checks.
2. Confirm physical and simulation data are valid and consistently processed.
3. Calculate the Euro NCAP assessment KPI(s).
4. Calculate KPI error.
5. Calculate ISO/TS 18571 rating for VUT longitudinal acceleration.
6. Compare against the current agreed acceptance thresholds.
7. Record pass/fail and root-cause notes.

For a scenario cluster, the VTA supporting protocol states that at least 75 percent of Euro NCAP verification tests per VTA cluster should satisfy the qualification criteria; otherwise, the simulation dossier is not accepted for that cluster. This is not confirmed as a CV rule in the local documents.

## 10. In-House Qualification Procedure

### 10.1 Plan the qualification campaign

For each intended CV frontal VTA scenario family:

1. Identify the CV protocol scenario and current physical assessment KPI.
2. Identify the passenger-car VTA cluster used by analogy.
3. Select qualification cases that cover corners and representative interior grid cells.
4. Include physical-test cases from the same vehicle configuration, sensor set, software, and loading condition as the virtual prediction campaign.
5. Define pass/fail thresholds before running the qualification comparison.
6. Freeze model versions and parameter sets.

Passenger-car VTA provides explicit in-house qualification examples for `CCRs`, `CCFtap`, `CPNA`, `CPNCO`, and lane ELK. It does not provide a CV case list. For vans, do not claim an official Euro NCAP case count unless a CV VTA document or Euro NCAP clarification is obtained.

### 10.2 Build the qualification dataset

Each physical and simulated run should include:

1. ISO-MME folder with `.mme`, channel files, movie files where applicable, and comment file.
2. Scenario setup sheet with nominal parameters and robustness layer if applicable.
3. Physical validity checklist.
4. Simulation configuration record.
5. Raw physical data and processed physical data.
6. Raw simulation output at 100 Hz.
7. Derived assessment KPI file.
8. ISO/TS 18571 result file.
9. Pass/fail summary and engineering notes.

### 10.3 Run physical tests

1. Prepare the van per CV protocol.
2. Conduct tests under valid track, weather, surroundings, and boundary conditions.
3. Record required data and metadata.
4. Determine T0, TAEB, TFCW, Timpact, and Tend.
5. Calculate physical assessment KPIs using CA 004 methods.

### 10.4 Run simulations

1. Use the same scenario parameters as the physical test.
2. Use the same VUT geometry, mass/load state, target definitions, and environmental assumptions.
3. Export required channels at 100 Hz.
4. Do not add simulation output filtering.
5. Verify physical plausibility and channel-code consistency.

### 10.5 Compare physical and simulated results

For each paired run:

1. Align coordinate systems and time bases.
2. Align TAEB for ISO score calculation.
3. Calculate ISO score over the defined interval.
4. Calculate KPI errors.
5. Record whether ISO and KPI criteria are met.
6. If criteria are not met, classify root cause:
   - scenario setup mismatch,
   - physical-test invalidity,
   - data-processing error,
   - model fidelity issue,
   - sensor/function-model mismatch,
   - target/environment mismatch,
   - threshold not applicable to CV.

### 10.6 Decide model qualification status

Recommended internal statuses:

| Status | Meaning |
| --- | --- |
| Qualified | All agreed ISO and KPI criteria met for the scenario cluster. |
| Qualified with limitation | Criteria met only within a defined speed, impact-location, target, ODD, or function boundary. |
| Not qualified | Criteria not met or dataset insufficient. |
| Pending Euro NCAP clarification | Local docs do not define a CV rule or threshold. |

## 11. Performance Prediction and Euro NCAP Dossier

### 11.1 Current CV prediction requirements

The CV frontal protocol requires manufacturers to provide color data for each grid point across all scenarios before testing, for AEB and FCW where applicable. Expected impact speeds are not required in the current CV color-data submission.

For van-to-car head-on scenarios (`VCFhos` and `VCFhol`), the CV protocol requires a dossier that includes at least:

1. Expected performance, including warning TTC where applicable, AEB activation TTC, and speed reduction.
2. System architecture, sensor setup, sensor fusion, and decision-making logic.
3. Operational conditions and limitations, including speed range, relative speed, overlap range, lighting/environmental limits, recognized vehicle types, lane requirements, and marking requirements.
4. System override conditions, such as accelerator, brake, steering angle, or steering rate.
5. Evidence of system verification, such as physical tests, HiL, SiL, or ViL.
6. Real-world performance evidence, including false-positive likelihood and mitigation strategy.

For a virtual-test dossier, add:

1. Simulation toolchain overview.
2. Vehicle model version and release notes.
3. Target model versions.
4. Environment model version.
5. Sensor model placement and assumptions.
6. Perfect-perception assumptions if used.
7. Function algorithm model description.
8. Qualification matrix and pass/fail summary.
9. KPI comparison tables.
10. ISO/TS 18571 result tables.
11. Known limitations and ODD exclusions.

### 11.2 Verification tests

Current CV frontal verification logic:

1. For `VCRs` AEB, `VCRs` FCW, and `VCRm`, verification is based on OEM grid prediction. Euro NCAP selects random grid cells distributed according to predicted color distribution, excluding red points.
2. The vehicle sponsor funds 15 verification tests where applicable: 10 AEB tests for `VCRs` and `VCRm`, and 5 FCW tests for `VCRs`.
3. The manufacturer may sponsor additional verification tests.
4. For `VCRb` and `VCFtap`, verification tests are performed at all test points.
5. For `VCCscp`, verification tests are performed at all test points where enough performance to score points is predicted.
6. CV PTW and VRU scenarios have their own speed-progression and stopping rules in the CV frontal protocol.

Current passenger-car VTA/frontal logic:

1. Standard and extended range verification tests are randomly selected per scenario.
2. Verification covers standard range, extended range, and robustness layers.
3. For virtual predictions, failure to meet VTA acceptance criteria causes scenarios in that cluster to be treated as self-claim for scoring.

For vans, do not merge these two approaches without Euro NCAP confirmation. Use the CV verification rules for current CV scoring, and use passenger-car verification logic only as a planning reference for anticipated VTA expansion.

## 12. Scoring for Dossier Calculations

### 12.1 Current CV frontal score calculation

The CV frontal protocol currently uses color-band scoring for each grid cell:

| Predicted color | Grid-cell sub-score |
| --- | --- |
| Green | 1.00 |
| Yellow | 0.75 |
| Orange | 0.50 |
| Brown | 0.25 |
| Red | 0.00 |

Scenario score:

```text
Scenario score =
  (sum of grid-cell sub-scores * total scenario score)
  / number of grid cells in the scenario
```

The result is rounded to the hundredth. Verification-test outcomes dictate the final score.

For `VCRs` and `VCRm` only, the CV protocol applies separate correction factors for AEB and FCW/AES:

```text
Correction factor = actual tested score / predicted score
```

The correction factor is applied to the relevant `VCRs` and `VCRm` function scores. The final score cannot exceed 100 percent of the available score regardless of a correction factor above 1.

### 12.2 Passenger-car standard range score, for future CV reference

The passenger-car protocol calculates standard range score by applying the same color sub-scores as above, then normalizing to the scenario's standard-range point allocation:

```text
Standard range score =
  (sum of standard-range sub-scores * total standard-range score)
  / number of standard-range grid cells
```

This split is not defined for CV frontal in the local CV protocol.

### 12.3 Passenger-car extended range score, for future CV reference

In the passenger-car protocol, extended range uses a binary color score:

| Predicted color | Extended-range sub-score |
| --- | --- |
| Green, Yellow, Orange, Brown | 1.00 |
| Red | 0.00 |

First calculate:

```text
Extended range raw score =
  (sum of extended-range sub-scores * total extended-range score)
  / number of extended-range grid cells
```

Then convert to the final extended-range score band:

| Raw extended score as percent of available extended score | Final extended score |
| --- | --- |
| 50 percent to less than 75 percent | 50 percent |
| 75 percent to less than 100 percent | 75 percent |
| 100 percent | 100 percent |

This is passenger-car logic only until a CV standard/extended definition exists.

### 12.4 Passenger-car robustness layer score, for future CV reference

The passenger-car protocol only allows robustness-layer points for a scenario if the standard range score reaches at least 50 percent of the available standard-range score.

For each scenario, the score per applicable robustness layer is:

```text
Robustness layer score =
  total robustness score / number of applicable robustness layers
```

Robustness predictions are split into:

| Robustness category | Examples | Evidence source in passenger-car protocol |
| --- | --- | --- |
| Decision and Control | Driver input pre-crash; target speed; acceleration; initial position offset; trajectory/heading | Virtual testing or self-claim, depending on layer. |
| Perception | Target type/appearance; adverse weather; illumination; infrastructure/clutter; obstruction/obscuration | Field data or Euro NCAP-requested verification, depending on layer. |

Use `ca_002_verification_conditions_for_robustness_layers_v11_4be423649d.pdf` for passenger-car robustness conditions. Do not present those conditions as CV-specific unless Euro NCAP confirms CV applicability.

### 12.5 Verification outcome scaling, passenger-car VTA reference

The passenger-car frontal protocol includes different score percentages from verification outcomes depending on:

1. Whether predictions came from Virtual Testing or self-claim.
2. Whether the range is standard or extended.
3. How many verification tests were selected.
4. How many tests passed, meaning the result was in line with or better than predicted.

Important passenger-car rule: if Virtual Testing predictions fail the VTA acceptance criteria, the whole scenario cluster is treated as self-claim for scoring. This is a useful internal risk rule for CV planning, but the local CV frontal protocol does not yet define this mechanism.

## 13. Internal Deliverables Checklist

Prepare the following folders or files for each scenario cluster:

| Deliverable | Contents |
| --- | --- |
| `00_sources` | Local protocol versions and applicability matrix. |
| `01_scenario_matrix` | CV grid cells, passenger-car analog, selected physical qualification cases, selected virtual prediction cases. |
| `02_physical_data` | ISO-MME physical runs, videos, validity checklists, environmental logs. |
| `03_simulation_data` | ISO-MME virtual runs, model metadata, tool versions, assumptions. |
| `04_processing` | Filtering record, derived KPI scripts/results, ISO/TS 18571 scripts/results. |
| `05_qualification` | Pairwise comparison table, KPI errors, ISO scores, pass/fail decision. |
| `06_predictions` | Color predictions for all required CV grid cells. |
| `07_dossier` | System architecture, ODD, override conditions, validation evidence, real-world evidence where required. |
| `08_gaps` | Open questions, manual research items, Euro NCAP clarification log. |

Recommended pairwise comparison table columns:

| Column | Description |
| --- | --- |
| Scenario | CV scenario code and passenger-car analog. |
| Grid cell | VUT speed, target speed, impact location, headway, acceleration, approach side. |
| Physical run ID | ISO-MME folder/run identifier. |
| Virtual run ID | ISO-MME folder/run identifier. |
| Physical valid? | Yes/no and reason. |
| KPI physical | Value and unit. |
| KPI simulation | Value and unit. |
| KPI error | Physical minus simulation. |
| ISO score | ISO/TS 18571 rating for VUT longitudinal acceleration. |
| Threshold source | CV, VTA, passenger-car fallback, or internal provisional. |
| Result | Pass/fail/pending clarification. |
| Notes | Root cause, limitation, or follow-up. |

## 14. Missing or External Information Requiring Manual Research

The following items are referenced or needed but are not available as complete van-specific instructions inside this directory:

1. Van-specific Euro NCAP VTA protocol for CV frontal collision avoidance.
   - Not found in this directory.
   - Manual research or Euro NCAP clarification required.

2. Confirmed CV VTA acceptance thresholds for ISO score and KPI errors.
   - The available VTA protocol contains bracketed values and says bracketed values require confirmation.
   - Manual research or Euro NCAP clarification required.

3. CV standard range, extended range, and robustness layer definitions for frontal collision avoidance.
   - The CV frontal protocol v1.0 does not contain the passenger-car standard/extended/robustness scoring split.
   - Manual research or Euro NCAP clarification required.

4. Approved ISO-MME scenario/type/subtype codes for CV frontal scenarios such as `VCRs`, `VCRm`, `VCRb`, `VCFtap`, and `VCCscp`.
   - CA 004 v1.4 in this directory appears to list passenger-car scenario codes only.
   - Manual research or Euro NCAP clarification required before final upload automation.

5. Euro NCAP VTA server upload schema, automated scripts, and detailed acceptance report format.
   - The VTA protocol says the VT server calculates criteria automatically, but the server/scripts are not in this directory.
   - Manual research or Euro NCAP access required.

6. Parameterized OpenSCENARIO files for Euro NCAP scenarios.
   - The VTA and passenger-car frontal protocols state that Euro NCAP will provide OpenSCENARIO files, including through GitHub for some scenarios.
   - These files are not in this directory.
   - Manual research or Euro NCAP/GitHub access required.

7. Technical Bulletin CP 004.
   - The passenger-car Application of Star Rating protocol references CP 004 for ISO-MME data, but CP 004 is not in this directory.
   - CA 004 is present and appears to cover Safe Driving and Crash Avoidance data acquisition, but the CP/CA reference mismatch should be checked manually.

8. MUSE project deliverable D2.1 for real motorcycle target resemblance.
   - Referenced by the CV frontal protocol definition of real motorcycle, but not in this directory.
   - Manual research required if real motorcycle target substitution is used.

9. Current official Euro NCAP supplier list / TB029 naming.
   - A suppliers-list PDF is present as `g_003_1_euro_ncap_suppliers_list_v411_22ae2877ed.pdf`, while the CV frontal protocol references TB029.
   - Confirm whether this local file is the current equivalent before procurement or test execution.

## 15. Recommended Internal Rule Until CV VTA Is Published

Use the following decision rule in internal reviews:

1. If the requirement is stated in the CV frontal protocol, follow the CV requirement.
2. If the requirement is stated only in the passenger-car frontal or VTA protocol, use it as provisional guidance and label it "passenger-car reference, pending CV confirmation."
3. If the requirement depends on bracketed values in Euro NCAP text, do not freeze it as an internal pass/fail threshold without confirmation.
4. If data formatting is required, use CA 004 and ISO/TS 13499, but confirm CV scenario-code support before external submission.
5. If a scenario is used in a score claim, retain physical evidence, simulation evidence, KPI comparison, ISO score comparison, and a traceable rationale for any passenger-car-to-CV mapping.
