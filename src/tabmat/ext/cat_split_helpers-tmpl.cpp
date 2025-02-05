#include <vector>


<%def name="transpose_matvec(dropfirst)">
template <typename F>
void _transpose_matvec_${dropfirst}(
    int n_rows,
    int* indices,
    F* other,
    F* res,
    int res_size
) {
    #pragma omp parallel
    {
        std::vector<F> restemp(res_size, 0.0);
        #pragma omp for
        for (int i = 0; i < n_rows; i++) {
            % if dropfirst == 'all_rows_drop_first':
                int col_idx = indices[i] - 1;
                if (col_idx != -1) {
                    restemp[col_idx] += other[i];
                }
            % else:
                restemp[indices[i]] += other[i];
            % endif
        }
        for (int i = 0; i < res_size; i++) {
            # pragma omp atomic
            res[i] += restemp[i];
        }
    }
}
</%def>


template <typename F>
void _sandwich_cat_cat(
    F* d,
    const int* i_indices,
    const int* j_indices,
    int* rows,
    int len_rows,
    F* res,
    int res_n_col,
    int res_size,
    bool i_drop_first,
    bool j_drop_first
)
{
    #pragma omp parallel
    {
        std::vector<F> restemp(res_size, 0.0);
        # pragma omp for
        for (int k_idx = 0; k_idx < len_rows; k_idx++) {
            int k = rows[k_idx];
            int i = i_indices[k] - i_drop_first;
            if (i == -1) {
                continue;
            }
            int j = j_indices[k] - j_drop_first;
            if (j == -1) {
                continue;
            }
            restemp[i * res_n_col + j] += d[k];
        }
        for (int i = 0; i < res_size; i++) {
            # pragma omp atomic
            res[i] += restemp[i];
        }
    }
}


<%def name="sandwich_cat_dense_tmpl(order)">
template <typename F>
void _sandwich_cat_dense${order}(
    F* d,
    const int* indices,
    int* rows,
    int len_rows,
    int* j_cols,
    int len_j_cols,
    F* res,
    int res_size,
    F* mat_j,
    int mat_j_nrow,
    int mat_j_ncol
    )
{
    #pragma omp parallel
    {
        std::vector<F> restemp(res_size, 0.0);
        #pragma omp for
        for (int k_idx = 0; k_idx < len_rows; k_idx++) {
            int k = rows[k_idx];
            int i = indices[k];
            // MAYBE TODO: explore whether the column restriction slows things down a
            // lot, particularly if not restricting the columns allows using SIMD
            // instructions
            // MAYBE TODO: explore whether swapping the loop order for F-ordered mat_j
            // is useful.
            for (int j_idx = 0; j_idx < len_j_cols; j_idx++) {
                int j = j_cols[j_idx];
                % if order == 'C':
                    restemp[i * len_j_cols + j_idx] += d[k] * mat_j[k * mat_j_ncol + j];
                % else:
                    restemp[i * len_j_cols + j_idx] += d[k] * mat_j[j * mat_j_nrow + k];
                % endif
            }
        }
        for (int i = 0; i < res_size; i++) {
            #pragma omp atomic
            res[i] += restemp[i];
        }
    }
}
</%def>

${sandwich_cat_dense_tmpl('C')}
${sandwich_cat_dense_tmpl('F')}
${transpose_matvec('all_rows')}
${transpose_matvec('all_rows_drop_first')}
