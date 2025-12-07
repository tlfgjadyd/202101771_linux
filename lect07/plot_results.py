import pandas as pd
import matplotlib.pyplot as plt
import os
import re
from matplotlib import font_manager, rc  # 폰트 설정을 위해 추가

# --- Matplotlib 한글 폰트 설정 시작 ---
try:
    # Windows 기본 폰트인 맑은 고딕 설정 (다른 OS 사용자는 폰트 이름/경로를 변경해야 함)
    font_path = "C:/Windows/Fonts/malgun.ttf"
    if not os.path.exists(font_path):
        # 폰트가 없을 경우 다른 폰트를 사용하도록 지정하거나 에러 처리
        print("경고: 맑은 고딕 폰트를 찾을 수 없습니다. 다른 한글 폰트 경로를 확인하세요.")
        font_name = 'sans-serif'  # 기본 폰트 유지 (경고는 계속 뜰 수 있음)
    else:
        font_name = font_manager.FontProperties(fname=font_path).get_name()

    rc('font', family=font_name)
    plt.rcParams['axes.unicode_minus'] = False  # 마이너스 기호 깨짐 방지

except Exception as e:
    print(f"폰트 설정 중 오류 발생: {e}")
# --- Matplotlib 한글 폰트 설정 끝 ---


# 1. 시뮬레이션 결과 파일 설정
FILENAME_PATTERN = r'average_results_tq_(\d+)\.csv'
OUTPUT_FILENAME = 'rr_final_performance_summary.png'

# 모든 결과를 저장할 리스트
all_tq_results = []

# 2. 결과 파일 읽고 데이터프레임으로 변환 (이하 로직은 동일)
print("현재 디렉토리에서 평균 결과 파일들을 검색합니다...")

for filename in os.listdir('.'):
    match = re.search(FILENAME_PATTERN, filename)
    if match:
        filepath = os.path.join('.', filename)
        tq = int(match.group(1))

        try:
            df_tq = pd.read_csv(filepath, encoding='utf-8')

            result = df_tq.set_index('지표')['평균값']

            data_row = {
                'Time Quantum (TQ)': tq,
                'Avg Total Ticks': result.loc['평균 총 소요 시간 (Ticks)'],
                'Avg Context Switches': result.loc['평균 문맥 교환 수'],
                'Avg Turnaround Time': result.loc['평균 반환시간 (Ticks)'],
                'Avg Waiting Time': result.loc['평균 대기시간 (Ticks)']
            }
            all_tq_results.append(data_row)

        except FileNotFoundError:
            print(f"오류: {filename} 파일을 찾을 수 없습니다.")
        except KeyError as e:
            print(f"오류: {filename} 파일의 지표 이름이 잘못되었습니다. (누락된 지표: {e})")
        except Exception as e:
            print(f"파일 처리 중 오류 발생 {filename}: {e}")

# 3. 최종 데이터프레임 생성 및 정렬
if not all_tq_results:
    print("분석할 유효한 평균 결과 파일이 없습니다. C 코드를 실행하여 파일을 먼저 생성해주세요.")
    exit()

df_final = pd.DataFrame(all_tq_results).sort_values(by='Time Quantum (TQ)')

print("\n--- 통합 분석 결과 ---")
# tabulate 설치를 전제로 to_markdown 사용
try:
    from tabulate import tabulate

    print(df_final.to_markdown(index=False))
except ImportError:
    print(df_final.to_string(index=False))

# 4. 그래프 시각화 (로직 동일)
fig, axes = plt.subplots(nrows=1, ncols=2, figsize=(15, 6))

tqs = df_final['Time Quantum (TQ)'].astype(str)
bar_width = 0.35

# --- 서브플롯 1: 시간 효율성 (Total Ticks & Turnaround) ---
ax1 = axes[0]
x_indices = range(len(tqs))

rects_ticks = ax1.bar(
    [i - bar_width / 2 for i in x_indices],
    df_final['Avg Total Ticks'],
    bar_width,
    label='평균 총 소요 시간',
    color='#3F51B5',
    alpha=0.7
)

rects_tt = ax1.bar(
    [i + bar_width / 2 for i in x_indices],
    df_final['Avg Turnaround Time'],
    bar_width,
    label='평균 반환 시간 (Turnaround Time)',
    color='#FF9800',
    alpha=0.7
)

ax1.set_title('시간 효율성 지표 비교 (Total Ticks vs. Turnaround)', fontsize=14)
ax1.set_xlabel('Time Quantum (TQ)', fontsize=12)
ax1.set_ylabel('평균 시간 (Ticks)', fontsize=12)
ax1.set_xticks(x_indices)
ax1.set_xticklabels(tqs)
ax1.legend()
ax1.grid(axis='y', linestyle='--', alpha=0.5)

# --- 서브플롯 2: 오버헤드 및 대기 (Context Switches & Waiting) ---
ax2 = axes[1]

rects_cs = ax2.bar(
    [i - bar_width / 2 for i in x_indices],
    df_final['Avg Context Switches'],
    bar_width,
    label='평균 문맥 교환 수 (Context Switches)',
    color='#4CAF50',
    alpha=0.8
)

rects_wt = ax2.bar(
    [i + bar_width / 2 for i in x_indices],
    df_final['Avg Waiting Time'],
    bar_width,
    label='평균 대기 시간 (Waiting Time)',
    color='#F44336',
    alpha=0.8
)

ax2.set_title('오버헤드 및 공정성 지표 비교 (Switches vs. Waiting)', fontsize=14)
ax2.set_xlabel('Time Quantum (TQ)', fontsize=12)
ax2.set_ylabel('평균 값', fontsize=12)
ax2.set_xticks(x_indices)
ax2.set_xticklabels(tqs)
ax2.legend()
ax2.grid(axis='y', linestyle='--', alpha=0.5)


# 막대 위에 값 표시 함수
def autolabel(ax, rects):
    for rect in rects:
        height = rect.get_height()
        ax.annotate(f'{height:.2f}',
                    xy=(rect.get_x() + rect.get_width() / 2, height),
                    xytext=(0, 3),
                    textcoords="offset points",
                    ha='center', va='bottom',
                    fontsize=8)


autolabel(ax1, rects_ticks)
autolabel(ax1, rects_tt)
autolabel(ax2, rects_cs)
autolabel(ax2, rects_wt)

plt.suptitle(f'라운드 로빈 스케줄링 성능 분석 (TQ: {", ".join(tqs)})', fontsize=16, weight='bold')
plt.tight_layout(rect=[0, 0.03, 1, 0.95])
plt.savefig(OUTPUT_FILENAME)

print(f"\n✅ 시각화 완료! 결과는 [{OUTPUT_FILENAME}] 파일로 저장되었습니다.")