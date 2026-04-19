% 1. Definisi Koefisien (16 Taps)
Num = [-0.0484, 0.0316, 0.0663, 0.0161, -0.0762, -0.0418, 0.1838, 0.4172, ...
        0.4172, 0.1838, -0.0418, -0.0762, 0.0161, 0.0663, 0.0316, -0.0484];
        %% 

% 2. Membuat Data Random (1800 Sampel)
% Kita gunakan 'seed' agar hasil bisa diulang jika perlu
rng(42); 
data_asli = randn(1, 1800); % Sinyal random (Gaussian noise)

% 3. Proses Filtering
% Karena FIR, kita gunakan Denominator = 1
data_filter = filter(Num, 1, data_asli);

% 4. Visualisasi Grafik
figure('Color', 'w');

% Plot Sinyal Asli
subplot(2,1,1);
stem(data_asli, 'r', 'LineWidth', 1.1);
grid on;
title('Sinyal Asli (1800 Data Random)');
ylabel('Amplitudo');
xlabel('Sampel ke-n');

% Plot Perbandingan
subplot(2,1,2);
hold on;
plot(data_asli, '--r', 'DisplayName', 'Tanpa Filter');
plot(data_filter, 'b', 'LineWidth', 2, 'DisplayName', 'Dengan Filter');
grid on;
title('Perbandingan Sinyal: Tanpa Filter vs Dengan Filter');
ylabel('Amplitudo');
xlabel('Sampel ke-n');
legend show;

% Menampilkan statistik singkat di Command Window
fprintf('Rata-rata Sinyal Asli: %.4f\n', mean(data_asli));
fprintf('Rata-rata Sinyal Terfilter: %.4f\n', mean(data_filter));


% 5. Hitung dan Tampilkan Variansi Sinyal
fprintf('Variansi Sinyal Asli: %.4f\n', var(data_asli));
fprintf('Variansi Sinyal Terfilter: %.4f\n', var(data_filter));


